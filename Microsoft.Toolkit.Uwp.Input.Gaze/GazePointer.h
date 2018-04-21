//Copyright (c) Microsoft. All rights reserved. Licensed under the MIT license.
//See LICENSE in the project root for license information.

#pragma once
#pragma warning(disable:4453)

#include "IGazeFilter.h"
#include "GazeCursor.h"
#include "GazePointerState.h"

using namespace Platform;
using namespace Platform::Collections;
using namespace Windows::Foundation;
using namespace Windows::Foundation::Collections;
using namespace Windows::Devices::Enumeration;
using namespace Windows::Devices::HumanInterfaceDevice;
using namespace Windows::UI::Core;
using namespace Windows::Devices::Input::Preview;

namespace Shapes = Windows::UI::Xaml::Shapes;

BEGIN_NAMESPACE_GAZE_INPUT

// units in microseconds
const TimeSpan DEFAULT_FIXATION_DELAY{ 4000000 };
const TimeSpan DEFAULT_DWELL_DELAY{ 8000000 };
const TimeSpan DEFAULT_REPEAT_DELAY{ 16000000 };
const TimeSpan DEFAULT_ENTER_EXIT_DELAY{ 500000 };
const TimeSpan DEFAULT_MAX_HISTORY_DURATION{ 30000000 };
const TimeSpan MAX_SINGLE_SAMPLE_DURATION{ 1000000 };

const TimeSpan GAZE_IDLE_TIME{ 25000000 };

ref struct GazeTargetItem;
ref struct GazeHistoryItem;

public ref class GazePointer sealed
{
public:
	virtual ~GazePointer();

	void LoadSettings(ValueSet^ settings);

	void InvokeTarget(UIElement^ target);
	void Reset();
	void SetElementStateDelay(UIElement ^element, GazePointerState pointerState, TimeSpan stateDelay);
	TimeSpan GetElementStateDelay(UIElement^ element, GazePointerState pointerState);

	// Provide a configurable delay for when the EyesOffDelay event is fired
	// GOTCHA: this value requires that _eyesOffTimer is instantiated so that it
	// can update the timer interval 
	property TimeSpan EyesOffDelay
	{
		TimeSpan get() { return _eyesOffDelay; }
		void set(TimeSpan value)
		{
			_eyesOffDelay = value;

			// convert GAZE_IDLE_TIME units (microseconds) to 100-nanosecond units used
			// by TimeSpan struct
			_eyesOffTimer->Interval = EyesOffDelay;
		}
	}

	// Pluggable filter for eye tracking sample data. This defaults to being set to the
	// NullFilter which performs no filtering of input samples.
	property IGazeFilter^ Filter;

	property bool IsCursorVisible
	{
		bool get() { return _gazeCursor->IsCursorVisible; }
		void set(bool value) { _gazeCursor->IsCursorVisible = value; }
	}

	property int CursorRadius
	{
		int get() { return _gazeCursor->CursorRadius; }
		void set(int value) { _gazeCursor->CursorRadius = value; }
	}

internal:

	static property GazePointer^ Instance{ GazePointer^ get(); }
	void OnPageUnloaded(Object^ sender, RoutedEventArgs^ args);
	EventRegistrationToken _unloadedToken;

	void AddRoot(FrameworkElement^ element);
	void RemoveRoot(FrameworkElement^ element);

private:

	GazePointer();

private:

	bool _isShuttingDown;

	TimeSpan GetDefaultPropertyValue(GazePointerState state);

	void    InitializeHistogram();
	void    InitializeGazeInputSource();
	void    DeinitializeGazeInputSource();

	GazeTargetItem^     GetOrCreateGazeTargetItem(UIElement^ target);
	GazeTargetItem^     GetGazeTargetItem(UIElement^ target);
	UIElement^          GetHitTarget(Point gazePoint);
	UIElement^          ResolveHitTarget(Point gazePoint, TimeSpan timestamp);

	bool    IsInvokable(UIElement^ target);

	void    CheckIfExiting(TimeSpan curTimestamp);
	void    GotoState(UIElement^ control, GazePointerState state);
	void    RaiseGazePointerEvent(UIElement^ target, GazePointerState state, TimeSpan elapsedTime);

	void OnGazeEntered(
		GazeInputSourcePreview^ provider,
		GazeEnteredPreviewEventArgs^ args);
	void OnGazeMoved(
		GazeInputSourcePreview^ provider,
		GazeMovedPreviewEventArgs^ args);
	void OnGazeExited(
		GazeInputSourcePreview^ provider,
		GazeExitedPreviewEventArgs^ args);

	void ProcessGazePoint(TimeSpan timestamp, Point position);

	void    OnEyesOff(Object ^sender, Object ^ea);

private:
	Vector<FrameworkElement^>^ _roots = ref new Vector<FrameworkElement^>();

	TimeSpan                               _eyesOffDelay;

	GazeCursor^                         _gazeCursor;
	DispatcherTimer^                    _eyesOffTimer;

	// _offScreenElement is a pseudo-element that represents the area outside
	// the screen so we can track how long the user has been looking outside
	// the screen and appropriately trigger the EyesOff event
	Control^                            _offScreenElement;

	// The value is the total time that FrameworkElement has been gazed at
	Vector<GazeTargetItem^>^            _activeHitTargetTimes;

	// A vector to track the history of observed gaze targets
	Vector<GazeHistoryItem^>^           _gazeHistory;
	TimeSpan                               _maxHistoryTime;

	// Used to determine if exit events need to be fired by adding GAZE_IDLE_TIME to the last 
	// saved timestamp
	TimeSpan                           _lastTimestamp;

	GazeInputSourcePreview^             _gazeInputSource;
	EventRegistrationToken              _gazeEnteredToken;
	EventRegistrationToken              _gazeMovedToken;
	EventRegistrationToken              _gazeExitedToken;
	CoreDispatcher^                     _coreDispatcher;

	TimeSpan _defaultFixation = DEFAULT_FIXATION_DELAY;
	TimeSpan _defaultDwell = DEFAULT_DWELL_DELAY;
	TimeSpan _defaultRepeat = DEFAULT_REPEAT_DELAY;
	TimeSpan _defaultEnter = DEFAULT_ENTER_EXIT_DELAY;
	TimeSpan _defaultExit = DEFAULT_ENTER_EXIT_DELAY;
};

END_NAMESPACE_GAZE_INPUT