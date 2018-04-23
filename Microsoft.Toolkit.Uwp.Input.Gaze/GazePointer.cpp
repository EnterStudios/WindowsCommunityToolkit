//Copyright (c) Microsoft. All rights reserved. Licensed under the MIT license.
//See LICENSE in the project root for license information.

#include "pch.h"
#include "GazePointer.h"
#include "GazeApi.h"
#include "GazeTargetItem.h"
#include "GazeHistoryItem.h"
#include "GazePointerEventArgs.h"
#include "GazeElement.h"
#include <xstddef>
#include <varargs.h>
#include <strsafe.h>

using namespace std;
using namespace Platform;
using namespace Windows::Foundation::Collections;
using namespace Windows::Graphics::Display;
using namespace Windows::UI;
using namespace Windows::UI::ViewManagement;
using namespace Windows::UI::Xaml::Automation::Peers;
using namespace Windows::UI::Xaml::Hosting;
using namespace Windows::UI::Xaml::Automation;
using namespace Windows::UI::Xaml::Automation::Provider;

BEGIN_NAMESPACE_GAZE_INPUT

GazePointer^ GazePointer::Instance::get()
{
	static auto value = ref new GazePointer();
	return value;
}

void GazePointer::AddRoot(FrameworkElement^ element)
{
	_roots->InsertAt(0, element);

	if (_roots->Size == 1)
	{
		_isShuttingDown = false;
		InitializeGazeInputSource();
	}
}

void GazePointer::RemoveRoot(FrameworkElement^ element)
{
	auto index = 0;
	while (index < _roots->Size && _roots->GetAt(index) != element)
	{
		index++;
	}
	if (index < _roots->Size)
	{
		_roots->RemoveAt(index);
	}

	if (_roots->Size == 0)
	{
		_isShuttingDown = true;
		_gazeCursor->IsGazeEntered = false;
		DeinitializeGazeInputSource();
	}
}

GazePointer::GazePointer()
{
	_coreDispatcher = CoreWindow::GetForCurrentThread()->Dispatcher;

	// Default to not filtering sample data
	Filter = ref new NullFilter();

	_gazeCursor = GazeCursor::Instance;

	// timer that gets called back if there gaze samples haven't been received in a while
	_eyesOffTimer = ref new DispatcherTimer();
	_eyesOffTimer->Tick += ref new EventHandler<Object^>(this, &GazePointer::OnEyesOff);

	// provide a default of GAZE_IDLE_TIME microseconds to fire eyes off 
	EyesOffDelay = GAZE_IDLE_TIME;

	InitializeHistogram();
}

GazePointer::~GazePointer()
{
	if (_gazeInputSource != nullptr)
	{
		_gazeInputSource->GazeEntered -= _gazeEnteredToken;
		_gazeInputSource->GazeMoved -= _gazeMovedToken;
		_gazeInputSource->GazeExited -= _gazeExitedToken;
	}
}

void GazePointer::LoadSettings(ValueSet^ settings)
{
	_gazeCursor->LoadSettings(settings);
	Filter->LoadSettings(settings);

	// TODO Add logic to protect against missing settings

	if (settings->HasKey("GazePointer.FixationDelay"))
	{
		_defaultFixation = TimeSpan{ 10 * (int)(settings->Lookup("GazePointer.FixationDelay")) };
	}

	if (settings->HasKey("GazePointer.DwellDelay"))
	{
		_defaultDwell = TimeSpan{ 10 * (int)(settings->Lookup("GazePointer.DwellDelay")) };
	}

	if (settings->HasKey("GazePointer.RepeatDelay"))
	{
		_defaultRepeat = TimeSpan{ 10 * (int)(settings->Lookup("GazePointer.RepeatDelay")) };
	}

	if (settings->HasKey("GazePointer.EnterExitDelay"))
	{
		_defaultEnter = TimeSpan{ 10 * (int)(settings->Lookup("GazePointer.EnterExitDelay")) };
	}

	if (settings->HasKey("GazePointer.EnterExitDelay"))
	{
		_defaultExit = TimeSpan{ 10 * (int)(settings->Lookup("GazePointer.EnterExitDelay")) };
	}

	// TODO need to set fixation and dwell for all elements
	if (settings->HasKey("GazePointer.FixationDelay"))
	{
		SetElementStateDelay(_offScreenElement, GazePointerState::Fixation, TimeSpan{ (int)(settings->Lookup("GazePointer.FixationDelay")) });
	}
	if (settings->HasKey("GazePointer.DwellDelay"))
	{
		SetElementStateDelay(_offScreenElement, GazePointerState::Dwell, TimeSpan{ (int)(settings->Lookup("GazePointer.DwellDelay")) });
	}

	if (settings->HasKey("GazePointer.GazeIdleTime"))
	{
		EyesOffDelay = TimeSpan{ 10 * (int)(settings->Lookup("GazePointer.GazeIdleTime")) };
	}
}

void GazePointer::InitializeHistogram()
{
	_activeHitTargetTimes = ref new Vector<GazeTargetItem^>();

	_offScreenElement = ref new UserControl();
	SetElementStateDelay(_offScreenElement, GazePointerState::Fixation, _defaultFixation);
	SetElementStateDelay(_offScreenElement, GazePointerState::Dwell, _defaultDwell);

	_maxHistoryTime = DEFAULT_MAX_HISTORY_DURATION;    // maintain about 3 seconds of history (in microseconds)
	_gazeHistory = ref new Vector<GazeHistoryItem^>();
}

void GazePointer::InitializeGazeInputSource()
{
	_gazeInputSource = GazeInputSourcePreview::GetForCurrentView();
	if (_gazeInputSource != nullptr)
	{
		_gazeEnteredToken = _gazeInputSource->GazeEntered += ref new TypedEventHandler<
			GazeInputSourcePreview^, GazeEnteredPreviewEventArgs^>(this, &GazePointer::OnGazeEntered);
		_gazeMovedToken = _gazeInputSource->GazeMoved += ref new TypedEventHandler<
			GazeInputSourcePreview^, GazeMovedPreviewEventArgs^>(this, &GazePointer::OnGazeMoved);
		_gazeExitedToken = _gazeInputSource->GazeExited += ref new TypedEventHandler<
			GazeInputSourcePreview^, GazeExitedPreviewEventArgs^>(this, &GazePointer::OnGazeExited);
	}
}

void GazePointer::DeinitializeGazeInputSource()
{
	if (_gazeInputSource != nullptr)
	{
		_gazeInputSource->GazeEntered -= _gazeEnteredToken;
		_gazeInputSource->GazeMoved -= _gazeMovedToken;
		_gazeInputSource->GazeExited -= _gazeExitedToken;
	}
}

static DependencyProperty^ GetProperty(GazePointerState state)
{
	switch (state)
	{
	case GazePointerState::Fixation: return GazeApi::FixationProperty;
	case GazePointerState::Dwell: return GazeApi::DwellProperty;
	case GazePointerState::DwellRepeat: return GazeApi::DwellRepeatProperty;
	case GazePointerState::Enter: return GazeApi::EnterProperty;
	case GazePointerState::Exit: return GazeApi::ExitProperty;
	default: return nullptr;
	}
}

TimeSpan GazePointer::GetDefaultPropertyValue(GazePointerState state)
{
	switch (state)
	{
	case GazePointerState::Fixation: return _defaultFixation;
	case GazePointerState::Dwell: return _defaultDwell;
	case GazePointerState::DwellRepeat: return _defaultRepeat;
	case GazePointerState::Enter: return _defaultEnter;
	case GazePointerState::Exit: return _defaultExit;
	default: throw ref new NotImplementedException();
	}
}

void GazePointer::SetElementStateDelay(UIElement ^element, GazePointerState relevantState, TimeSpan stateDelay)
{
	auto property = GetProperty(relevantState);
	element->SetValue(property, stateDelay);

	// fix up _maxHistoryTime in case the new param exceeds the history length we are currently tracking
	auto dwellTime = GetElementStateDelay(element, GazePointerState::Dwell);
	auto repeatTime = GetElementStateDelay(element, GazePointerState::DwellRepeat);
	_maxHistoryTime = 2 * max(dwellTime, repeatTime);
}

TimeSpan GazePointer::GetElementStateDelay(UIElement ^element, GazePointerState pointerState)
{
	auto property = GetProperty(pointerState);

	DependencyObject^ walker = element;
	Object^ valueAtWalker = walker->GetValue(property);

	while (GazeApi::UnsetTimeSpan.Equals(valueAtWalker) && walker != nullptr)
	{
		walker = VisualTreeHelper::GetParent(walker);

		if (walker != nullptr)
		{
			valueAtWalker = walker->GetValue(property);
		}
	}

	auto ticks = GazeApi::UnsetTimeSpan.Equals(valueAtWalker) ? GetDefaultPropertyValue(pointerState) : safe_cast<TimeSpan>(valueAtWalker);

	switch (pointerState)
	{
	case GazePointerState::Dwell:
	case GazePointerState::DwellRepeat:
		_maxHistoryTime = max(_maxHistoryTime, 2 * ticks);
		break;
	}
	return ticks;
}

void GazePointer::Reset()
{
	_activeHitTargetTimes->Clear();
	_gazeHistory->Clear();

	_maxHistoryTime = DEFAULT_MAX_HISTORY_DURATION;
}

GazeTargetItem^ GazePointer::GetHitTarget(Point gazePoint)
{
    for each (auto rootElement in _roots)
    {
        auto targets = VisualTreeHelper::FindElementsInHostCoordinates(gazePoint, rootElement, false);
        GazeTargetItem^ invokable = nullptr;
        for each (auto target in targets)
        {
            if (invokable == nullptr)
            {
                auto item = GazeTargetItem::GetOrCreate(target);
                if (item->IsInvokable)
                {
                    invokable = item;
                }
            }

			switch (GazeApi::GetIsGazeEnabled(target))
			{
			case GazeEnablement::Enabled:
				if (invokable != nullptr)
				{
					return invokable;
				}
				break;

            case GazeEnablement::Disabled:
                return GazeTargetItem::NonInvokable;
            }
        }
        assert(invokable == nullptr);
    }
    // TODO : Check if the location is offscreen
    return GazeTargetItem::NonInvokable;
}

void GazePointer::ActivateGazeTargetItem(GazeTargetItem^ target)
{
    unsigned int index;
    if (!_activeHitTargetTimes->IndexOf(target, &index))
    {
        _activeHitTargetTimes->Append(target);

		// calculate the time that the first DwellRepeat needs to be fired after. this will be updated every time a DwellRepeat is 
		// fired to keep track of when the next one is to be fired after that.
		auto nextStateTime = GetElementStateDelay(target->TargetElement, GazePointerState::Enter);

        target->Reset(nextStateTime);
    }
}

GazeTargetItem^ GazePointer::ResolveHitTarget(Point gazePoint, TimeSpan timestamp)
{
	// TODO: The existance of a GazeTargetItem should be used to indicate that
	// the target item is invokable. The method of invokation should be stored
	// within the GazeTargetItem when it is created and not recalculated when
	// subsequently needed.

    // create GazeHistoryItem to deal with this sample
    auto target = GetHitTarget(gazePoint);
    auto historyItem = ref new GazeHistoryItem();
    historyItem->HitTarget = target;
    historyItem->Timestamp = timestamp;
    historyItem->Duration = 0;
    assert(historyItem->HitTarget != nullptr);

    // create new GazeTargetItem with a (default) total elapsed time of zero if one does not exist already.
    // this ensures that there will always be an entry for target elements in the code below.
    ActivateGazeTargetItem(target);
    target->LastTimestamp = timestamp;

	// find elapsed time since we got the last hit target
	historyItem->Duration = timestamp - _lastTimestamp;
	if (historyItem->Duration > MAX_SINGLE_SAMPLE_DURATION)
	{
		historyItem->Duration = MAX_SINGLE_SAMPLE_DURATION;
	}
	_gazeHistory->Append(historyItem);

	// update the time this particular hit target has accumulated
	target->DetailedTime += historyItem->Duration;

	// drop the oldest samples from the list until we have samples only 
	// within the window we are monitoring
	//
	// historyItem is the last item we just appended a few lines above. 
	for (auto evOldest = _gazeHistory->GetAt(0);
		historyItem->Timestamp - evOldest->Timestamp > _maxHistoryTime;
		evOldest = _gazeHistory->GetAt(0))
	{
		_gazeHistory->RemoveAt(0);

		// subtract the duration obtained from the oldest sample in _gazeHistory
		auto targetItem = GetGazeTargetItem(evOldest->HitTarget);
		assert((targetItem->DetailedTime - evOldest->Duration).Duration >= 0);
		targetItem->DetailedTime -= evOldest->Duration;
		if (targetItem->ElementState != GazePointerState::PreEnter)
		{
			targetItem->OverflowTime += evOldest;
		}
	}

	_lastTimestamp = timestamp;

    // Return the most recent hit target 
    // Intuition would tell us that we should return NOT the most recent
    // hitTarget, but the one with the most accumulated time in 
    // in the maintained history. But the effect of that is that
    // the user will feel that they have clicked on the wrong thing
    // when they are looking at something else.
    // That is why we return the most recent hitTarget so that 
    // when its dwell time has elapsed, it will be invoked
    return target;
}

void GazePointer::GotoState(UIElement^ control, GazePointerState state)
{
	Platform::String^ stateName;

	switch (state)
	{
	case GazePointerState::Enter:
		return;
	case GazePointerState::Exit:
		stateName = "Normal";
		break;
	case GazePointerState::Fixation:
		stateName = "Fixation";
		break;
	case GazePointerState::DwellRepeat:
	case GazePointerState::Dwell:
		stateName = "Dwell";
		break;
	default:
		assert(0);
		return;
	}

	// TODO: Implement proper support for visual states
	// VisualStateManager::GoToState(dynamic_cast<Control^>(control), stateName, true);
}

void GazePointer::OnEyesOff(Object ^sender, Object ^ea)
{
	_eyesOffTimer->Stop();

	CheckIfExiting(_lastTimestamp + EyesOffDelay);
	RaiseGazePointerEvent(nullptr, GazePointerState::Enter, EyesOffDelay);
}

void GazePointer::CheckIfExiting(TimeSpan curTimestamp)
{
	for (unsigned int index = 0; index < _activeHitTargetTimes->Size; index++)
	{
		auto targetItem = _activeHitTargetTimes->GetAt(index);
		auto targetElement = targetItem->TargetElement;
		auto exitDelay = GetElementStateDelay(targetElement, GazePointerState::Exit);

        long long idleDuration = curTimestamp - targetItem->LastTimestamp;
        if (targetItem->ElementState != GazePointerState::PreEnter && idleDuration > exitDelay)
        {
            targetItem->ElementState = GazePointerState::PreEnter;
            GotoState(targetElement, GazePointerState::Exit);
            RaiseGazePointerEvent(targetItem, GazePointerState::Exit, targetItem->ElapsedTime);
            targetItem->GiveFeedback();

			_activeHitTargetTimes->RemoveAt(index);

            // remove all history samples referring to deleted hit target
            for (unsigned i = 0; i < _gazeHistory->Size; )
            {
                auto hitTarget = _gazeHistory->GetAt(i)->HitTarget;
                if (hitTarget->TargetElement == targetElement)
                {
                    _gazeHistory->RemoveAt(i);
                }
                else
                {
                    i++;
                }
            }

			// return because only one element can be exited at a time and at this point
			// we have done everything that we can do
			return;
		}
	}
}

wchar_t *PointerStates[] = {
	L"Exit",
	L"PreEnter",
	L"Enter",
	L"Fixation",
	L"Dwell",
	L"DwellRepeat"
};

void GazePointer::RaiseGazePointerEvent(GazeTargetItem^ target, GazePointerState state, TimeSpan elapsedTime)
{
    auto control = target != nullptr ? safe_cast<Control^>(target->TargetElement) : nullptr;
    //assert(target != _rootElement);
    auto gpea = ref new GazePointerEventArgs(control, state, elapsedTime);
    //auto buttonObj = dynamic_cast<Button ^>(target);
    //if (buttonObj && buttonObj->Content)
    //{
    //    String^ buttonText = dynamic_cast<String^>(buttonObj->Content);
    //    Debug::WriteLine(L"GPE: %s -> %s, %d", buttonText, PointerStates[(int)state], elapsedTime);
    //}
    //else
    //{
    //    Debug::WriteLine(L"GPE: 0x%08x -> %s, %d", target != nullptr ? target->GetHashCode() : 0, PointerStates[(int)state], elapsedTime);
    //}

    auto gazeElement = target != nullptr ? GazeApi::GetGazeElement(control) : nullptr;

	if (gazeElement != nullptr)
	{
		gazeElement->RaiseStateChanged(control, gpea);
	}

	if (state == GazePointerState::Dwell)
	{
		auto handled = false;

		if (gazeElement != nullptr)
		{
			auto args = ref new GazeInvokedRoutedEventArgs();
			gazeElement->RaiseInvoked(control, args);
			handled = args->Handled;
		}

        if (!handled)
        {
            target->Invoke();
        }
    }
}

void GazePointer::OnGazeEntered(GazeInputSourcePreview^ provider, GazeEnteredPreviewEventArgs^ args)
{
	Debug::WriteLine(L"Entered at %ld", args->CurrentPoint->Timestamp);
	_gazeCursor->IsGazeEntered = true;
}

void GazePointer::OnGazeMoved(GazeInputSourcePreview^ provider, GazeMovedPreviewEventArgs^ args)
{
	if (!_isShuttingDown)
	{
		auto intermediatePoints = args->GetIntermediatePoints();
		for each(auto point in intermediatePoints)
		{
			auto position = point->EyeGazePosition;
			if (position != nullptr)
			{
				_gazeCursor->IsGazeEntered = true;
				ProcessGazePoint(TimeSpan{ 10 * (int64)point->Timestamp }, position->Value);
			}
			else
			{
				Debug::WriteLine(L"Null position eaten at %ld", point->Timestamp);
			}
		}
	}
}

void GazePointer::OnGazeExited(GazeInputSourcePreview^ provider, GazeExitedPreviewEventArgs^ args)
{
	Debug::WriteLine(L"Exited at %ld", args->CurrentPoint->Timestamp);
	_gazeCursor->IsGazeEntered = false;
}

void GazePointer::ProcessGazePoint(TimeSpan timestamp, Point position)
{
	auto ea = ref new GazeEventArgs(position, timestamp);

	auto fa = Filter->Update(ea);
	_gazeCursor->Position = fa->Location;

    auto targetItem = ResolveHitTarget(fa->Location, fa->Timestamp);
    assert(targetItem != nullptr);

	//Debug::WriteLine(L"ProcessGazePoint: %llu -> [%d, %d], %llu", hitTarget->GetHashCode(), (int)fa->Location.X, (int)fa->Location.Y, fa->Timestamp);

	// check to see if any element in _hitTargetTimes needs an exit event fired.
	// this ensures that all exit events are fired before enter event
	CheckIfExiting(fa->Timestamp);

    GazePointerState nextState = static_cast<GazePointerState>(static_cast<int>(targetItem->ElementState) + 1);

	Debug::WriteLine(L"%llu -> State=%d, Elapsed=%d, NextStateTime=%d", targetItem->TargetElement, targetItem->ElementState, targetItem->ElapsedTime, targetItem->NextStateTime);

	if (targetItem->ElapsedTime > targetItem->NextStateTime)
	{
		auto prevStateTime = targetItem->NextStateTime;

		// prevent targetItem from ever actually transitioning into the DwellRepeat state so as
		// to continuously emit the DwellRepeat event
		if (nextState != GazePointerState::DwellRepeat)
		{
			targetItem->ElementState = nextState;
			nextState = static_cast<GazePointerState>(static_cast<int>(nextState) + 1);     // nextState++
			targetItem->NextStateTime = GetElementStateDelay(targetItem->TargetElement, nextState);
		}
		else
		{
			// move the NextStateTime by one dwell period, while continuing to stay in Dwell state
			targetItem->NextStateTime += GetElementStateDelay(targetItem->TargetElement, GazePointerState::Dwell) -
				GetElementStateDelay(targetItem->TargetElement, GazePointerState::Fixation);
		}

		if (targetItem->ElementState == GazePointerState::Dwell)
		{
			targetItem->RepeatCount++;
			if (targetItem->MaxRepeatCount < targetItem->RepeatCount)
			{
				targetItem->NextStateTime = TimeSpan{ MAXINT64 };
			}
		}

		GotoState(targetItem->TargetElement, targetItem->ElementState);

        RaiseGazePointerEvent(targetItem, targetItem->ElementState, targetItem->ElapsedTime);
    }

	targetItem->GiveFeedback();

	_eyesOffTimer->Start();
	_lastTimestamp = fa->Timestamp;
}

END_NAMESPACE_GAZE_INPUT