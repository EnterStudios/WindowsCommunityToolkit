//Copyright (c) Microsoft. All rights reserved. Licensed under the MIT license.
//See LICENSE in the project root for license information.

#include "pch.h"
#include "GazeApi.h"
#include "GazePointer.h"
#include "GazePointerProxy.h"
#include "GazeElement.h"

using namespace std;
using namespace Platform;
using namespace Windows::Foundation;
using namespace Windows::Foundation::Collections;
using namespace Windows::Graphics::Display;
using namespace Windows::UI;
using namespace Windows::UI::ViewManagement;
using namespace Windows::UI::Xaml::Automation::Peers;
using namespace Windows::UI::Xaml::Hosting;
using namespace Windows::UI::Xaml::Media;

BEGIN_NAMESPACE_GAZE_INPUT

static Brush^ s_progressBrush = ref new SolidColorBrush(Colors::Green);

Brush^ GazeApi::GazeFeedbackProgressBrush::get()
{
    return s_progressBrush;
}

void GazeApi::GazeFeedbackCompleteBrush::set(Brush^ value)
{
    s_progressBrush = value;
}

static Brush^ s_completeBrush = ref new SolidColorBrush(Colors::Red);

Brush^ GazeApi::GazeFeedbackCompleteBrush::get()
{
    return s_completeBrush;
}

void GazeApi::GazeFeedbackProgressBrush::set(Brush^ value)
{
    s_completeBrush = value;
}

TimeSpan GazeApi::UnsetTimeSpan = { -1 };

static void OnIsGazeEnabledChanged(DependencyObject^ ob, DependencyPropertyChangedEventArgs^ args)
{
    auto element = safe_cast<FrameworkElement^>(ob);
    auto isGazeEnabled = safe_cast<GazeEnablement>(args->NewValue);
    GazePointerProxy::SetGazeEnabled(element, isGazeEnabled);
}

static void OnIsGazeCursorVisibleChanged(DependencyObject^ ob, DependencyPropertyChangedEventArgs^ args)
{
    GazePointer::Instance->IsCursorVisible = safe_cast<bool>(args->NewValue);
}

static void OnGazeCursorRadiusChanged(DependencyObject^ ob, DependencyPropertyChangedEventArgs^ args)
{
    GazePointer::Instance->CursorRadius = safe_cast<int>(args->NewValue);
}

static DependencyProperty^ s_isGazeEnabledProperty = DependencyProperty::RegisterAttached("IsGazeEnabled", GazeEnablement::typeid, GazeApi::typeid,
    ref new PropertyMetadata(GazeEnablement::Inherited, ref new PropertyChangedCallback(&OnIsGazeEnabledChanged)));
static DependencyProperty^ s_isGazeCursorVisibleProperty = DependencyProperty::RegisterAttached("IsGazeCursorVisible", bool::typeid, GazeApi::typeid,
    ref new PropertyMetadata(true, ref new PropertyChangedCallback(&OnIsGazeCursorVisibleChanged)));
static DependencyProperty^ s_gazeCursorRadiusProperty = DependencyProperty::RegisterAttached("GazeCursorRadius", int::typeid, GazeApi::typeid,
    ref new PropertyMetadata(6, ref new PropertyChangedCallback(&OnGazeCursorRadiusChanged)));
static DependencyProperty^ s_gazeElementProperty = DependencyProperty::RegisterAttached("GazeElement", GazeElement::typeid, GazeApi::typeid, ref new PropertyMetadata(nullptr));
static DependencyProperty^ s_fixationProperty = DependencyProperty::RegisterAttached("Fixation", TimeSpan::typeid, GazeApi::typeid, ref new PropertyMetadata(GazeApi::UnsetTimeSpan));
static DependencyProperty^ s_dwellProperty = DependencyProperty::RegisterAttached("Dwell", TimeSpan::typeid, GazeApi::typeid, ref new PropertyMetadata(GazeApi::UnsetTimeSpan));
static DependencyProperty^ s_dwellRepeatProperty = DependencyProperty::RegisterAttached("DwellRepeat", TimeSpan::typeid, GazeApi::typeid, ref new PropertyMetadata(GazeApi::UnsetTimeSpan));
static DependencyProperty^ s_enterProperty = DependencyProperty::RegisterAttached("Enter", TimeSpan::typeid, GazeApi::typeid, ref new PropertyMetadata(GazeApi::UnsetTimeSpan));
static DependencyProperty^ s_exitProperty = DependencyProperty::RegisterAttached("Exit", TimeSpan::typeid, GazeApi::typeid, ref new PropertyMetadata(GazeApi::UnsetTimeSpan));
static DependencyProperty^ s_maxRepeatCountProperty = DependencyProperty::RegisterAttached("MaxRepeatCount", int::typeid, GazeApi::typeid, ref new PropertyMetadata(safe_cast<Object^>(0)));

DependencyProperty^ GazeApi::IsGazeEnabledProperty::get() { return s_isGazeEnabledProperty; }
DependencyProperty^ GazeApi::IsGazeCursorVisibleProperty::get() { return s_isGazeCursorVisibleProperty; }
DependencyProperty^ GazeApi::GazeCursorRadiusProperty::get() { return s_gazeCursorRadiusProperty; }
DependencyProperty^ GazeApi::GazeElementProperty::get() { return s_gazeElementProperty; }
DependencyProperty^ GazeApi::FixationProperty::get() { return s_fixationProperty; }
DependencyProperty^ GazeApi::DwellProperty::get() { return s_dwellProperty; }
DependencyProperty^ GazeApi::DwellRepeatProperty::get() { return s_dwellRepeatProperty; }
DependencyProperty^ GazeApi::EnterProperty::get() { return s_enterProperty; }
DependencyProperty^ GazeApi::ExitProperty::get() { return s_exitProperty; }
DependencyProperty^ GazeApi::MaxRepeatCountProperty::get() { return s_maxRepeatCountProperty; }

GazeEnablement GazeApi::GetIsGazeEnabled(UIElement^ element) { return safe_cast<GazeEnablement>(element->GetValue(s_isGazeEnabledProperty)); }
bool GazeApi::GetIsGazeCursorVisible(UIElement^ element) { return safe_cast<bool>(element->GetValue(s_isGazeCursorVisibleProperty)); }
int GazeApi::GetGazeCursorRadius(UIElement^ element) { return safe_cast<int>(element->GetValue(s_gazeCursorRadiusProperty)); }
GazeElement^ GazeApi::GetGazeElement(UIElement^ element) { return safe_cast<GazeElement^>(element->GetValue(s_gazeElementProperty)); }
TimeSpan GazeApi::GetFixation(UIElement^ element) { return safe_cast<TimeSpan>(element->GetValue(s_fixationProperty)); }
TimeSpan GazeApi::GetDwell(UIElement^ element) { return safe_cast<TimeSpan>(element->GetValue(s_dwellProperty)); }
TimeSpan GazeApi::GetDwellRepeat(UIElement^ element) { return safe_cast<TimeSpan>(element->GetValue(s_dwellRepeatProperty)); }
TimeSpan GazeApi::GetEnter(UIElement^ element) { return safe_cast<TimeSpan>(element->GetValue(s_enterProperty)); }
TimeSpan GazeApi::GetExit(UIElement^ element) { return safe_cast<TimeSpan>(element->GetValue(s_exitProperty)); }
int GazeApi::GetMaxRepeatCount(UIElement^ element) { return safe_cast<int>(element->GetValue(s_maxRepeatCountProperty)); }

void GazeApi::SetIsGazeEnabled(UIElement^ element, GazeEnablement value) { element->SetValue(s_isGazeEnabledProperty, value); }
void GazeApi::SetIsGazeCursorVisible(UIElement^ element, bool value) { element->SetValue(s_isGazeCursorVisibleProperty, value); }
void GazeApi::SetGazeCursorRadius(UIElement^ element, int value) { element->SetValue(s_gazeCursorRadiusProperty, value); }
void GazeApi::SetGazeElement(UIElement^ element, GazeElement^ value) { element->SetValue(s_gazeElementProperty, value); }
void GazeApi::SetFixation(UIElement^ element, TimeSpan span) { element->SetValue(s_fixationProperty, span); }
void GazeApi::SetDwell(UIElement^ element, TimeSpan span) { element->SetValue(s_dwellProperty, span); }
void GazeApi::SetDwellRepeat(UIElement^ element, TimeSpan span) { element->SetValue(s_dwellRepeatProperty, span); }
void GazeApi::SetEnter(UIElement^ element, TimeSpan span) { element->SetValue(s_enterProperty, span); }
void GazeApi::SetExit(UIElement^ element, TimeSpan span) { element->SetValue(s_exitProperty, span); }
void GazeApi::SetMaxRepeatCount(UIElement^ element, int value) { element->SetValue(s_maxRepeatCountProperty, value); }

GazePointer^ GazeApi::GetGazePointer(Page^ page)
{
    return GazePointer::Instance;
}

END_NAMESPACE_GAZE_INPUT