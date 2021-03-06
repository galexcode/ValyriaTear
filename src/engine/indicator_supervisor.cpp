////////////////////////////////////////////////////////////////////////////////
//            Copyright (C) 2004-2011 by The Allacrost Project
//            Copyright (C) 2012-2014 by Bertram (Valyria Tear)
//                         All Rights Reserved
//
// This code is licensed under the GNU GPL version 2. It is free software and
// you may modify it and/or redistribute it under the terms of this license.
// See http://www.gnu.org/copyleft/gpl.html for details.
////////////////////////////////////////////////////////////////////////////////

/** ****************************************************************************
*** \file    indicator_supervisor.h
*** \author  Tyler Olsen, roots@allacrost.org
*** \author  Yohann Ferreira, yohann ferreira orange fr
*** \brief   Source file for battle indicator displays.
*** ***************************************************************************/

#include "utils/utils_pch.h"
#include "indicator_supervisor.h"

#include "engine/system.h"
#include "engine/video/video.h"
#include "common/global/global.h"

#include "utils/utils_random.h"
#include "utils/utils_strings.h"

using namespace vt_video;

namespace vt_mode_manager
{

//! \brief The total amount of time (in milliseconds) that the display sequence lasts for indicator elements
const uint32 INDICATOR_TIME = 3000;

//! \brief The amount of time (in milliseconds) that indicator elements fade at the beginning of the display sequence
const uint32 INDICATOR_FADEIN_TIME = 500;

//! \brief The amount of time (in milliseconds) that indicator elements fade at the end of the display sequence
const uint32 INDICATOR_FADEOUT_TIME = 1000;

//! \brief Represents the initial y (up) force applied on the indicator effect
const float INITIAL_FORCE = 12.0f;

////////////////////////////////////////////////////////////////////////////////
// IndicatorElement class
////////////////////////////////////////////////////////////////////////////////

IndicatorElement::IndicatorElement(float x_position, float y_position, INDICATOR_TYPE indicator_type) :
    _timer(INDICATOR_TIME),
    _alpha_color(1.0f, 1.0f, 1.0f, 0.0f),
    _x_force(0.0f),
    _y_force(INITIAL_FORCE),
    _x_origin_position(x_position),
    _y_origin_position(y_position),
    _x_relative_position(0.0f),
    _y_relative_position(0.0f),
    _indicator_type(indicator_type)
{
}


void IndicatorElement::Start()
{
    if(!_timer.IsInitial())
        _timer.Reset();
    _timer.Run();

    // Reinit the indicator push
    _x_force = vt_utils::RandomFloat(-20.0f, 20.0f);
    _y_force = INITIAL_FORCE;
}

void IndicatorElement::Update()
{
    _timer.Update();
    _UpdateDrawPosition();
    _ComputeDrawAlpha();
}

void IndicatorElement::_UpdateDrawPosition()
{
    // the time passed since the last call in ms.
    float elapsed_ms = static_cast<float>(vt_system::SystemManager->GetUpdateTime());

    switch(_indicator_type) {
    case DAMAGE_INDICATOR: {
        // Indicator gravity appliance in pixels / seconds
        const float INDICATOR_WEIGHT = 26.0f;

        _y_force -= elapsed_ms / 1000 * INDICATOR_WEIGHT;

        // Compute a potential maximum fall speed
        if(_y_force < -15.0f)
            _y_force = -15.0f;

        _y_relative_position += _y_force;

        // Resolve a ground collision
        if(_y_relative_position <= 0.0f) {
            _y_relative_position = 0.0f;

            // If the force is very low, the bouncing is over
            if(std::abs(_y_force) <= INDICATOR_WEIGHT / 10.0f) {
                _y_force = 0.0f;
            } else {
                // Make the object bounce
                _y_force = -(_y_force * 0.6f);
            }
        }

        // Make the object advance only if it still can.
        if(_y_relative_position > 0.0f)
            _x_relative_position += (_x_force / 1000 * elapsed_ms);
    }
    break;

    default:
    case HEALING_INDICATOR:
    case POSITIVE_STATUS_EFFECT_INDICATOR:
        _y_relative_position += 5.0f / 1000 * elapsed_ms;
        break;
    case ITEM_INDICATOR:
    case NEGATIVE_STATUS_EFFECT_INDICATOR:
        _y_relative_position -= 5.0f / 1000 * elapsed_ms;
        break;
    case TEXT_INDICATOR:
        // Move vertically
        _x_relative_position -= 5.0f / 1000 * elapsed_ms;
        break;
    }
}

void IndicatorElement::_ComputeDrawAlpha()
{
    // Timer is not running nor paused so indicator should not be drawn
    if((_timer.GetState() == vt_system::SYSTEM_TIMER_RUNNING) && (_timer.GetState() == vt_system::SYSTEM_TIMER_PAUSED)) {
        _alpha_color.SetAlpha(0.0f);
    }
    // Timer is in beginning stage and indicator graphic is fading in
    else if(_timer.GetTimeExpired() < INDICATOR_FADEIN_TIME) {
        _alpha_color.SetAlpha(static_cast<float>(_timer.GetTimeExpired()) / static_cast<float>(INDICATOR_FADEIN_TIME));
    }
    // Timer is in final stage and indicator graphic is fading out
    else if(_timer.TimeLeft() < INDICATOR_FADEOUT_TIME) {
        _alpha_color.SetAlpha(static_cast<float>(_timer.TimeLeft()) / static_cast<float>(INDICATOR_FADEOUT_TIME));
    }
    // Timer is in middle stage and indicator graphic should be drawn with no transparency
    else {
        _alpha_color.SetAlpha(1.0f);
    }
}

////////////////////////////////////////////////////////////////////////////////
// IndicatorText class
////////////////////////////////////////////////////////////////////////////////

IndicatorText::IndicatorText(float x_position, float y_position,
                             const std::string& text, const vt_video::TextStyle& style,
                             INDICATOR_TYPE indicator_type) :
    IndicatorElement(x_position, y_position, indicator_type),
    _text_image(text, style)
{}



void IndicatorText::Draw()
{
    VideoManager->SetDrawFlags(VIDEO_X_RIGHT, VIDEO_Y_BOTTOM, VIDEO_BLEND, 0);
    VideoManager->Move(_x_origin_position + _x_relative_position, _y_origin_position - _y_relative_position);

    _text_image.Draw(_alpha_color);
}

////////////////////////////////////////////////////////////////////////////////
// IndicatorImage class
////////////////////////////////////////////////////////////////////////////////

IndicatorImage::IndicatorImage(float x_position, float y_position, const std::string &filename,
                               INDICATOR_TYPE indicator_type) :
    IndicatorElement(x_position, y_position, indicator_type)
{
    if(!_image.Load(filename))
        PRINT_WARNING << "Failed to load indicator image: " << filename << std::endl;
}



IndicatorImage::IndicatorImage(float x_position, float y_position, const StillImage &image,
                               INDICATOR_TYPE indicator_type) :
    IndicatorElement(x_position, y_position, indicator_type),
    _image(image)
{
    if (_image.GetFilename().empty())
        PRINT_WARNING << "Invalid indicator image." << std::endl;
}


void IndicatorImage::Draw()
{
    VideoManager->SetDrawFlags(VIDEO_X_RIGHT, VIDEO_Y_BOTTOM, VIDEO_BLEND, 0);
    VideoManager->Move(_x_origin_position + _x_relative_position, _y_origin_position - _y_relative_position);

    _image.Draw(_alpha_color);
}

////////////////////////////////////////////////////////////////////////////////
// IndicatorBlendedImage class
////////////////////////////////////////////////////////////////////////////////

IndicatorBlendedImage::IndicatorBlendedImage(float x_position, float y_position,
                                             const std::string& first_filename,
                                             const std::string& second_filename,
                                             INDICATOR_TYPE indicator_type) :
    IndicatorElement(x_position, y_position, indicator_type),
    _second_alpha_color(1.0f, 1.0f, 1.0f, 0.0f)
{
    if(!_first_image.Load(first_filename))
        PRINT_WARNING << "Invalid first indicator image." << std::endl;
    if(!_second_image.Load(second_filename))
        PRINT_WARNING << "Invalid second indicator image." << std::endl;
}



IndicatorBlendedImage::IndicatorBlendedImage(float x_position, float y_position,
                                             const StillImage& first_image,
                                             const StillImage& second_image,
                                             INDICATOR_TYPE indicator_type) :
    IndicatorElement(x_position, y_position, indicator_type),
    _first_image(first_image),
    _second_image(second_image),
    _second_alpha_color(1.0f, 1.0f, 1.0f, 0.0f)
{
    if(_first_image.GetFilename().empty())
        PRINT_WARNING << "Invalid first indicator image." << std::endl;
    if(_second_image.GetFilename().empty())
        PRINT_WARNING << "Invalid first indicator image." << std::endl;
}



void IndicatorBlendedImage::Draw()
{
    VideoManager->SetDrawFlags(VIDEO_X_RIGHT, VIDEO_Y_BOTTOM, VIDEO_BLEND, 0);
    VideoManager->Move(_x_origin_position + _x_relative_position, _y_origin_position - _y_relative_position);

    // Initial fade in of first image
    if(_timer.GetTimeExpired() <= INDICATOR_FADEIN_TIME) {
        _first_image.Draw(_alpha_color);
    }
    // Opaque draw of first image
    else if(_timer.GetTimeExpired() <= INDICATOR_TIME / 4) {
        _first_image.Draw();
    }
    // Blended draw of first and second images
    else if(_timer.GetTimeExpired() <= INDICATOR_TIME / 2) {
        _alpha_color.SetAlpha(static_cast<float>((INDICATOR_TIME / 2) - _timer.GetTimeExpired())
                              / static_cast<float>(1000));
        _second_alpha_color.SetAlpha(1.0f - _alpha_color.GetAlpha());
        _first_image.Draw(_alpha_color);
        _second_image.Draw(_second_alpha_color);
    }
    // Opaque draw of second image
    else if(_timer.GetTimeExpired() <= INDICATOR_TIME / 3 * 2) {
        _second_image.Draw();
    }
    // Final fade out of second image
    else { // <= end
        _second_image.Draw(_alpha_color);
    }
}

////////////////////////////////////////////////////////////////////////////////
// IndicatorSupervisor class
////////////////////////////////////////////////////////////////////////////////

IndicatorSupervisor::~IndicatorSupervisor()
{
    for(uint32 i = 0; i < _wait_queue.size(); ++i)
        delete _wait_queue[i];
    _wait_queue.clear();

    for(uint32 i = 0; i < _active_queue.size(); ++i)
        delete _active_queue[i];
    _active_queue.clear();
}

static bool IndicatorCompare(IndicatorElement *one, IndicatorElement *another)
{
    return (one->GetXOrigin() > another->GetXOrigin());
}

void IndicatorSupervisor::Update()
{
    // Update all active elements
    for(uint32 i = 0; i < _active_queue.size(); i++)
        _active_queue[i]->Update();

    // Remove all expired elements from the active queue
    while(_active_queue.empty() == false) {
        if(_active_queue.front()->IsExpired() == true) {
            delete _active_queue.front();
            _active_queue.pop_front();
        } else {
            // If the front element is not expired, no other elements should be expired either
            break;
        }
    }

    bool must_sort = false;
    while(!_wait_queue.empty()) {

        // Update the element position if it is overlapping another one.
        _wait_queue.front()->Start(); // Setup the indcator's coords
        while(_FixPotentialIndicatorOverlapping(_wait_queue.front()))
            {}

        _active_queue.push_back(_wait_queue.front());
        _wait_queue.pop_front();
        must_sort = true;
    }

    // Sort the indicator display in that case
    if(must_sort)
        std::sort(_active_queue.begin(), _active_queue.end(), IndicatorCompare);
}

bool IndicatorSupervisor::_FixPotentialIndicatorOverlapping(IndicatorElement *element)
{
    if(!element)
        return false; // No overlapping

    IndicatorElement *overlapped_element = 0;

    // Get potential overlapped indicators
    for(std::deque<IndicatorElement *>::iterator it = _active_queue.begin(),
            it_end = _active_queue.end(); it != it_end; ++it) {
        if((*it)->GetXOrigin() == element->GetXOrigin() &&
           (*it)->GetYOrigin() == element->GetYOrigin()) {
            overlapped_element = *it;
            break;
        }
    }

    if(!overlapped_element)
        return false; // No overlapping

    // Move the next indicator a bit depending on its type
    if(element->GetType() == DAMAGE_INDICATOR)
        element->SetXOrigin(element->GetXOrigin() + 1.0f);
    else
        element->SetXOrigin(element->GetXOrigin() + 15.0f);
    return true;
}

void IndicatorSupervisor::Draw()
{
    for(uint32 i = 0; i < _active_queue.size(); i++)
        _active_queue[i]->Draw();
}

void IndicatorSupervisor::AddDamageIndicator(float x_position, float y_position,
                                             uint32 amount, const TextStyle& style)
{
    if (amount == 0)
        return;

    std::string text = vt_utils::NumberToString(amount);

    _wait_queue.push_back(new IndicatorText(x_position, y_position, text, style, DAMAGE_INDICATOR));
}



void IndicatorSupervisor::AddHealingIndicator(float x_position, float y_position,
                                              uint32 amount, const TextStyle& style)
{
    if(amount == 0)
        return;

    std::string text = vt_utils::NumberToString(amount);

    _wait_queue.push_back(new IndicatorText(x_position, y_position, text, style, HEALING_INDICATOR));
}

void IndicatorSupervisor::AddMissIndicator(float x_position, float y_position)
{
    std::string text = vt_system::Translate("Miss");
    TextStyle style("text24", Color::white);
    _wait_queue.push_back(new IndicatorText(x_position, y_position, text, style, TEXT_INDICATOR));
}

void IndicatorSupervisor::AddStatusIndicator(float x_position, float y_position,
                                             vt_global::GLOBAL_STATUS status, vt_global::GLOBAL_INTENSITY old_intensity,
                                             vt_global::GLOBAL_INTENSITY new_intensity)
{
    // If the status and intensity has not changed, only a single status icon needs to be used
    if(old_intensity == new_intensity) {
        StillImage *image = vt_global::GlobalManager->Media().GetStatusIcon(status, new_intensity);
        _wait_queue.push_back(new IndicatorImage(x_position, y_position, *image, POSITIVE_STATUS_EFFECT_INDICATOR));
    }
    // Otherwise two status icons need to be used in the indicator image
    else {
        StillImage *first_image = vt_global::GlobalManager->Media().GetStatusIcon(status, old_intensity);
        StillImage *second_image = vt_global::GlobalManager->Media().GetStatusIcon(status, new_intensity);
        INDICATOR_TYPE indicator_type = (old_intensity <= new_intensity) ?
                                        POSITIVE_STATUS_EFFECT_INDICATOR : NEGATIVE_STATUS_EFFECT_INDICATOR;
        _wait_queue.push_back(new IndicatorBlendedImage(x_position, y_position, *first_image, *second_image, indicator_type));
    }
}

void IndicatorSupervisor::AddItemIndicator(float x_position, float y_position, const vt_global::GlobalItem& item)
{
    _wait_queue.push_back(new IndicatorImage(x_position, y_position, item.GetIconImage(),
                                             ITEM_INDICATOR));
}

} // namespace vt_mode_manager
