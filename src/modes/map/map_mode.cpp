///////////////////////////////////////////////////////////////////////////////
//            Copyright (C) 2004-2011 by The Allacrost Project
//            Copyright (C) 2012-2014 by Bertram (Valyria Tear)
//                         All Rights Reserved
//
// This code is licensed under the GNU GPL version 2. It is free software
// and you may modify it and/or redistribute it under the terms of this license.
// See http://www.gnu.org/copyleft/gpl.html for details.
///////////////////////////////////////////////////////////////////////////////

/** ****************************************************************************
*** \file    map_mode.cpp
*** \author  Tyler Olsen, roots@allacrost.org
*** \author  Yohann Ferreira, yohann ferreira orange fr
*** \brief   Source file for map mode interface.
*** ***************************************************************************/

#include "utils/utils_pch.h"
#include "modes/map/map_mode.h"

#include "modes/map/map_dialogue.h"
#include "modes/map/map_events.h"
#include "modes/map/map_objects.h"
#include "modes/map/map_sprites.h"
#include "modes/map/map_tiles.h"

#include "modes/menu/menu.h"
#include "modes/pause.h"
#include "modes/boot/boot.h"
#include "modes/save/save_mode.h"

#include "engine/audio/audio.h"
#include "engine/input.h"

#include "common/global/global.h"

using namespace vt_utils;
using namespace vt_audio;
using namespace vt_boot;
using namespace vt_input;
using namespace vt_mode_manager;
using namespace vt_script;
using namespace vt_system;
using namespace vt_video;
using namespace vt_global;
using namespace vt_menu;
using namespace vt_pause;
using namespace vt_save;
using namespace vt_map::private_map;

namespace vt_map
{

// Initialize static class variables
MapMode *MapMode::_current_instance = NULL;

// ****************************************************************************
// ********** MapMode Public Class Methods
// ****************************************************************************

MapMode::MapMode(const std::string &data_filename, const std::string& script_filename) :
    GameMode(),
    _activated(false),
    _map_data_filename(data_filename),
    _map_script_filename(script_filename),
    _tile_supervisor(NULL),
    _object_supervisor(NULL),
    _event_supervisor(NULL),
    _dialogue_supervisor(NULL),
    _treasure_supervisor(NULL),
    _camera_x_in_map_corner(false),
    _camera_y_in_map_corner(false),
    _camera(NULL),
    _delta_x(0),
    _delta_y(0),
    _num_map_contexts(0),
    _running_disabled(false),
    _unlimited_stamina(false),
    _show_gui(true),
    _run_stamina(10000),
    _gui_alpha(0.0f),
    _minimap(NULL),
    _show_minimap(false)
{
    mode_type = MODE_MANAGER_MAP_MODE;
    _current_instance = this;

    ResetState();
    PushState(STATE_EXPLORE);

    // Load miscellaneous map graphics
    _dialogue_icon.LoadFromAnimationScript("img/misc/dialogue_icon.lua");
    ScaleToMapCoords(_dialogue_icon);

    // Load the save point animation files.
    AnimatedImage anim;
    anim.LoadFromAnimationScript("img/misc/save_point/save_point3.lua");
    active_save_point_animations.push_back(anim);

    anim.Clear();
    anim.LoadFromAnimationScript("img/misc/save_point/save_point2.lua");
    active_save_point_animations.push_back(anim);

    anim.Clear();
    anim.LoadFromAnimationScript("img/misc/save_point/save_point1.lua");
    inactive_save_point_animations.push_back(anim);

    anim.Clear();
    anim.LoadFromAnimationScript("img/misc/save_point/save_point2.lua");
    inactive_save_point_animations.push_back(anim);

    // Transform the animation size to correspond to the map coordinates system.
    for(uint32 i = 0; i < active_save_point_animations.size(); ++i)
        ScaleToMapCoords(active_save_point_animations[i]);

    for(uint32 i = 0; i < inactive_save_point_animations.size(); ++i)
        ScaleToMapCoords(inactive_save_point_animations[i]);

    _tile_supervisor = new TileSupervisor();
    _object_supervisor = new ObjectSupervisor();
    _event_supervisor = new EventSupervisor();
    _dialogue_supervisor = new DialogueSupervisor();
    _treasure_supervisor = new TreasureSupervisor();

    _intro_timer.Initialize(4000, 0);
    _intro_timer.EnableAutoUpdate(this);

    _camera_timer.Initialize(0, 1);

    if(!_Load()) {
        BootMode *BM = new BootMode();
        ModeManager->PopAll();
        ModeManager->Push(BM);
        return;
    }

    // Once the minimap file has been set (in the load function),
    // we can create the minimap
    if(_show_minimap)
        _CreateMinimap();

    GlobalMedia& media = GlobalManager->Media();
    _stamina_bar_background = media.GetStaminaBarBackgroundImage();
    _stamina_bar = media.GetStaminaBarImage();
    _stamina_bar_infinite_overlay = media.GetStaminaInfiniteImage();

    // Init the script component.
    GetScriptSupervisor().Initialize(this);

    //! Init the camera position text style
    _debug_camera_position.SetStyle(TextStyle("title22", Color::white, VIDEO_TEXT_SHADOW_DARK));
}



MapMode::~MapMode()
{
    delete(_tile_supervisor);
    delete(_object_supervisor);
    delete(_event_supervisor);
    delete(_dialogue_supervisor);
    delete(_treasure_supervisor);
    if(_minimap) delete _minimap;
}

void MapMode::Deactivate()
{
    if (!_activated)
        return;

    // Store the music state (but only once)
    MusicDescriptor *active_music = AudioManager->GetActiveMusic();
    _music_filename = active_music ? active_music->GetFilename() : std::string();
    _audio_state = active_music ? active_music->GetState() : AUDIO_STATE_UNLOADED;

    _activated = false;
}

void MapMode::Reset()
{
    _current_instance = this;

    _activated = true;

    // Reset video engine context properties
    VideoManager->SetCoordSys(0.0f, SCREEN_GRID_X_LENGTH, SCREEN_GRID_Y_LENGTH, 0.0f);
    VideoManager->SetDrawFlags(VIDEO_X_CENTER, VIDEO_Y_BOTTOM, 0);

    // Make the map location known globally to other code that may need to know this information
    GlobalManager->SetMap(_map_data_filename, _map_script_filename,
                          _map_image.GetFilename(), _map_hud_name.GetString());

    _ResetMusicState();

    _intro_timer.Run();

    // Reset potential map scripts
    GetScriptSupervisor().Reset();

    // If the sprite is based on the battle formation, we'd better refresh the data now
    // if the game in in the default mode.
    // Still, we won't change the sprite in the middle of a scene for instance.
    // I.e: When going out of the menu mode.
    if(CurrentState() == private_map::STATE_EXPLORE)
        _object_supervisor->ReloadVisiblePartyMember();
}

void MapMode::_ResetMusicState()
{
    MusicDescriptor *music = AudioManager->RetrieveMusic(_music_filename);
    MusicDescriptor *active_music = AudioManager->GetActiveMusic();
    // Stop the current music if it's not the right one.
    if (active_music && music != active_music)
        active_music->FadeOut(500);

    // If there is no map music or the music is already in the correct state, don't do anything.
    if (!music || music->GetState() == _audio_state)
        return;

    switch(_audio_state) {
    case AUDIO_STATE_FADE_IN:
    case AUDIO_STATE_PLAYING:
    default:
        // TODO: Seek the music point to not restart it from the beginning.
        // music->SeekSecond(_audio_time_offset);
        // In case the music volume was modified, we fade it back in smoothly
        if(music->GetVolume() < 1.0f)
            music->FadeIn(1000);
        else
            music->Play();
        break;
    case AUDIO_STATE_UNLOADED:
    case AUDIO_STATE_FADE_OUT:
    case AUDIO_STATE_PAUSED:
    case AUDIO_STATE_STOPPED:
        if (music->GetState() == AUDIO_STATE_PLAYING || music->GetState() == AUDIO_STATE_FADE_IN)
            music->FadeOut(1000);
        break;
    }
}

void MapMode::Update()
{
    // Update the map frame coords
    // NOTE: It's done before handling pause so that the frame is updated at
    // least once before setting the pause mode, avoiding a crash.
    _UpdateMapFrame();

    // Process quit and pause events unconditional to the state of map mode
    if(InputManager->QuitPress()) {
        ModeManager->Push(new PauseMode(true));
        return;
    } else if(InputManager->PausePress()) {
        ModeManager->Push(new PauseMode(false));
        return;
    } else if(InputManager->MinimapPress()) {
        //! Toggles the minimap view as requested by the user.
        GlobalManager->ShowMinimap(!GlobalManager->ShouldShowMinimap());
        return;
    }

    _dialogue_icon.Update();

    // Call the map script's update function
    if(_update_function.is_valid())
        ScriptCallFunction<void>(_update_function);

    // Update all animated tile images
    _tile_supervisor->Update();
    _object_supervisor->Update();
    _object_supervisor->SortObjects();

    switch(CurrentState()) {
    case STATE_SCENE:
    case STATE_DIALOGUE:
        // Fade out the gui alpha
        if (_gui_alpha > 0.0f)
            _gui_alpha -= SystemManager->GetUpdateTime() * 0.005;
        break;
    default:
        // Fade in the gui alpha if necessary
        if (_gui_alpha < 1.0f)
            _gui_alpha += SystemManager->GetUpdateTime() * 0.005;
        break;
    }

    // Update the active state of the map
    switch(CurrentState()) {
    case STATE_EXPLORE:
        _UpdateExplore();
        break;
    case STATE_SCENE:
        // Nothing
        break;
    case STATE_DIALOGUE:
        _dialogue_supervisor->Update();
        break;
    case STATE_TREASURE:
        _camera->moving = false;
        _treasure_supervisor->Update();
        break;
    default:
        IF_PRINT_WARNING(MAP_DEBUG) << "map was set in an unknown state: "
                                    << CurrentState() << std::endl;
        ResetState();
        break;
    }

    // ---------- (4) Update the camera timer
    _camera_timer.Update();

    // ---------- (5) Update all active map events
    _event_supervisor->Update();

    //update collision camera
    if (_show_minimap && _minimap && (CurrentState() == STATE_EXPLORE)
            && GlobalManager->ShouldShowMinimap())
        _minimap->Update(_camera, _gui_alpha);

    GameMode::Update();

    // Updates the debug info if needed
    if(!VideoManager->DebugInfoOn())
        return;

    // Camera map coordinates
    VirtualSprite *cam = GetCamera();
    if(!cam)
        return;
    float x_pos = cam->GetXPosition();
    float y_pos = cam->GetYPosition();
    std::ostringstream coord_txt;
    coord_txt << "Camera position: " << x_pos << ", " << y_pos;
    _debug_camera_position.SetText(coord_txt.str());
} // void MapMode::Update()

void MapMode::Draw()
{
    VideoManager->SetStandardCoordSys();
    GetScriptSupervisor().DrawBackground();

    VideoManager->SetCoordSys(0.0f, SCREEN_GRID_X_LENGTH, SCREEN_GRID_Y_LENGTH, 0.0f);
    VideoManager->SetDrawFlags(VIDEO_X_CENTER, VIDEO_Y_BOTTOM, 0);
    _DrawMapLayers();

    VideoManager->SetStandardCoordSys();
    GetScriptSupervisor().DrawForeground();

    VideoManager->SetCoordSys(0.0f, SCREEN_GRID_X_LENGTH, SCREEN_GRID_Y_LENGTH, 0.0f);
    VideoManager->SetDrawFlags(VIDEO_X_CENTER, VIDEO_Y_BOTTOM, 0);
    _object_supervisor->DrawDialogIcons();
}

void MapMode::DrawPostEffects()
{
    VideoManager->SetCoordSys(0.0f, SCREEN_GRID_X_LENGTH, SCREEN_GRID_Y_LENGTH, 0.0f);
    VideoManager->SetDrawFlags(VIDEO_X_CENTER, VIDEO_Y_BOTTOM, 0);
    // Halos are additive blending made, so they should be applied
    // as post-effects but before the GUI.
    _object_supervisor->DrawLights();

    VideoManager->SetStandardCoordSys();
    GetScriptSupervisor().DrawPostEffects();

    VideoManager->SetCoordSys(0.0f, SCREEN_GRID_X_LENGTH, SCREEN_GRID_Y_LENGTH, 0.0f);
    VideoManager->SetDrawFlags(VIDEO_X_CENTER, VIDEO_Y_BOTTOM, 0);

    // Draw the gui, unaffected by potential fading effects.
    _DrawGUI();

    if(CurrentState() == STATE_DIALOGUE)
        _dialogue_supervisor->Draw();

    // Draw the treasure menu if necessary
    if(CurrentState() == STATE_TREASURE)
        _treasure_supervisor->Draw();
}

void MapMode::ResetState()
{
    _state_stack.clear();
    _state_stack.push_back(STATE_INVALID);
}



void MapMode::PushState(MAP_STATE state)
{
    _state_stack.push_back(state);
}



void MapMode::PopState()
{
    _state_stack.pop_back();
    if(_state_stack.empty() == true) {
        IF_PRINT_WARNING(MAP_DEBUG)
                << "stack was empty after operation, reseting state stack"
                << std::endl;
        _state_stack.push_back(STATE_INVALID);
    }
}



MAP_STATE MapMode::CurrentState()
{
    if(_state_stack.empty() == true) {
        IF_PRINT_WARNING(MAP_DEBUG) << "stack was empty, reseting state stack"
                                    << std::endl;
        _state_stack.push_back(STATE_INVALID);
    }
    return _state_stack.back();
}

void MapMode::AddFlatGroundObject(MapObject *obj)
{
    if(!obj) {
        PRINT_WARNING << "Couldn't add NULL object." << std::endl;
        return;
    }
    _object_supervisor->_flat_ground_objects.push_back(obj);
    _object_supervisor->_all_objects.insert(std::make_pair(obj->object_id, obj));
}

void MapMode::AddGroundObject(MapObject *obj)
{
    if(!obj) {
        PRINT_WARNING << "Couldn't add NULL object." << std::endl;
        return;
    }
    _object_supervisor->_ground_objects.push_back(obj);
    _object_supervisor->_all_objects.insert(std::make_pair(obj->object_id, obj));
}


void MapMode::AddPassObject(MapObject *obj)
{
    if(!obj) {
        PRINT_WARNING << "Couldn't add NULL object." << std::endl;
        return;
    }
    _object_supervisor->_pass_objects.push_back(obj);
    _object_supervisor->_all_objects.insert(std::make_pair(obj->object_id, obj));
}


void MapMode::AddSkyObject(MapObject *obj)
{
    if(!obj) {
        PRINT_WARNING << "Couldn't add NULL object." << std::endl;
        return;
    }
    _object_supervisor->_sky_objects.push_back(obj);
    _object_supervisor->_all_objects.insert(std::make_pair(obj->object_id, obj));
}

void MapMode::AddAmbientSoundObject(SoundObject *obj)
{
    if(!obj) {
        PRINT_WARNING << "Couldn't add NULL object." << std::endl;
        return;
    }

    _object_supervisor->_sound_objects.push_back(obj);
}


void MapMode::AddZone(MapZone *zone)
{
    if(!zone) {
        PRINT_WARNING << "Couldn't add NULL zone." << std::endl;
        return;
    }
    _object_supervisor->_zones.push_back(zone);
}


void MapMode::AddSavePoint(float x, float y)
{
    SavePoint *save_point = new SavePoint(x, y);
    _object_supervisor->_save_points.push_back(save_point);
}

void MapMode::AddHalo(const std::string &filename, float x, float y, const Color &color)
{
    Halo *halo = new Halo(filename, x, y, color);
    _object_supervisor->_halos.push_back(halo);
}

void MapMode::AddLight(const std::string &main_flare_filename,
                       const std::string &secondary_flare_filename,
                       float x, float y,
                       const Color &main_color,
                       const Color &secondary_color)
{
    Light *light = new Light(main_flare_filename,
                             secondary_flare_filename,
                             x, y, main_color,
                             secondary_color);

    AddLight(light);
}

void MapMode::AddLight(Light *light)
{
    _object_supervisor->_lights.push_back(light);
}

void MapMode::SetCamera(private_map::VirtualSprite *sprite, uint32 duration)
{
    if(_camera == sprite) {
        IF_PRINT_WARNING(MAP_DEBUG) << "Camera was moved to the same sprite"
                                    << std::endl;
    } else {
        if(duration > 0) {
            _delta_x = _camera->GetXPosition() - sprite->GetXPosition();
            _delta_y = _camera->GetYPosition() - sprite->GetYPosition();
            _camera_timer.Reset();
            _camera_timer.SetDuration(duration);
            _camera_timer.Run();
        }
        _camera = sprite;
    }
}

void MapMode::MoveVirtualFocus(float loc_x, float loc_y)
{
    _object_supervisor->VirtualFocus()->SetPosition(loc_x, loc_y);
}

void MapMode::MoveVirtualFocus(float loc_x, float loc_y, uint32 duration)
{
    if(_camera != _object_supervisor->VirtualFocus()) {
        IF_PRINT_WARNING(MAP_DEBUG)
                << "Attempt to move camera although on different sprite" << std::endl;
    } else {
        if(duration > 0) {
            _delta_x = _object_supervisor->VirtualFocus()->GetXPosition() - static_cast<float>(loc_x);
            _delta_y = _object_supervisor->VirtualFocus()->GetYPosition() - static_cast<float>(loc_y);
            _camera_timer.Reset();
            _camera_timer.SetDuration(duration);
            _camera_timer.Run();
        }
        MoveVirtualFocus(loc_x, loc_y);
    }
}


bool MapMode::IsCameraOnVirtualFocus()
{
    return _camera == _object_supervisor->VirtualFocus();
}

bool MapMode::AttackAllowed()
{
    return (CurrentState() == STATE_EXPLORE && !IsCameraOnVirtualFocus());
}

float MapMode::GetScreenXCoordinate(float tile_position_x) const
{
    return (tile_position_x - _map_frame.screen_edges.left)
        * VIDEO_STANDARD_RES_WIDTH / SCREEN_GRID_X_LENGTH;
}

float MapMode::GetScreenYCoordinate(float tile_position_y) const
{
    return (tile_position_y - _map_frame.screen_edges.top)
        * VIDEO_STANDARD_RES_HEIGHT / SCREEN_GRID_Y_LENGTH;
}

// ****************************************************************************
// ********** MapMode Private Class Methods
// ****************************************************************************

bool MapMode::_Load()
{
    // Map data
    // Clear out all old map data if existing.
    ScriptManager->DropGlobalTable("map_data");

    // Open map script file and read in the basic map properties and tile definitions
    if(!_map_script.OpenFile(_map_data_filename)) {
        PRINT_ERROR << "Couldn't open map data file: "
                    << _map_data_filename << std::endl;
        return false;
    }

    if(!_map_script.OpenTable("map_data")) {
        PRINT_ERROR << "Couldn't open table 'map_data' in: "
                    << _map_data_filename << std::endl;
        _map_script.CloseFile();
        return false;
    }

    // Loads the collision grid
    if(!_object_supervisor->Load(_map_script)) {
        PRINT_ERROR << "Failed to load the collision grid from: "
            << _map_data_filename << std::endl;
        _map_script.CloseFile();
        return false;
    }

    // Instruct the supervisor classes to perform their portion of the load operation
    if(!_tile_supervisor->Load(_map_script)) {
        PRINT_ERROR << "Failed to load the tile data from: "
            << _map_data_filename << std::endl;
        _map_script.CloseFile();
        return false;
    }

    _map_script.CloseAllTables();
    _map_script.CloseFile(); // Free the map data file once everyhting is loaded

    // Map script

    _map_script_tablespace = ScriptEngine::GetTableSpace(_map_script_filename);
    if(_map_script_tablespace.empty()) {
        PRINT_ERROR << "Invalid map script namespace in: "
                    << _map_script_filename << std::endl;
        return false;
    }

    // Clear out all old map data if existing.
    ScriptManager->DropGlobalTable(_map_script_tablespace);

    // Open map script file and read in the basic map properties and tile definitions
    if(!_map_script.OpenFile(_map_script_filename)) {
        PRINT_ERROR << "Couldn't open map script file: "
                    << _map_script_filename << std::endl;
        return false;
    }

    if(_map_script.OpenTablespace().empty()) {
        PRINT_ERROR << "Couldn't open map script namespace in: "
                    << _map_script_filename << std::endl;
        _map_script.CloseFile();
        return false;
    }

    // Loads the map image and translated location names.
    // Test for empty strings to never trigger the default gettext msg string
    // which contains translation info.
    std::string map_hud_name = _map_script.ReadString("map_name");
    _map_hud_name.SetText(map_hud_name.empty() ? ustring() : UTranslate(map_hud_name),
                          TextStyle("map_title"));
    std::string map_hud_subname = _map_script.ReadString("map_subname");
    _map_hud_subname.SetText(map_hud_subname.empty() ? ustring() : UTranslate(map_hud_subname),
                             TextStyle("title24"));

    std::string map_image_filename = _map_script.ReadString("map_image_filename");
    if(!map_image_filename.empty() && !_map_image.Load(map_image_filename))
        PRINT_ERROR << "Failed to load location graphic image: "
                    << map_image_filename << std::endl;

    // Load map default music
    // NOTE: Other audio handling will be handled through scripting
    _music_filename = _map_script.ReadString("music_filename");
    if(!_music_filename.empty() && !AudioManager->LoadMusic(_music_filename, this))
        PRINT_WARNING << "Failed to load map music: " << _music_filename << std::endl;
    else if (!_music_filename.empty())
        _audio_state = AUDIO_STATE_PLAYING; // Set the default music state to "playing".

    // Call the map script's custom load function and get a reference to all other script function pointers
    ScriptObject map_table(luabind::from_stack(_map_script.GetLuaState(), vt_script::private_script::STACK_TOP));
    ScriptObject function = map_table["Load"];

    bool loading_succeeded = true;
    if(function.is_valid()) {
        try {
            ScriptCallFunction<void>(function, this);
        } catch(const luabind::error &e) {
            ScriptManager->HandleLuaError(e);
            loading_succeeded = false;
        } catch(const luabind::cast_failed &e) {
            ScriptManager->HandleCastError(e);
            loading_succeeded = false;
        }
    } else {
        loading_succeeded = false;
    }

    if(!loading_succeeded) {
        PRINT_ERROR << "Invalid map Load() function in "
                    << _map_script_filename
                    << ". The function wasn't called."
                    << std::endl;
        _map_script.CloseAllTables();
        _map_script.CloseFile();
        return false;
    }

    _update_function = _map_script.ReadFunctionPointer("Update");

    _map_script.CloseAllTables();
    _map_script.CloseFile(); // Free the map script file once everything is loaded

    return true;
} // bool MapMode::_Load()

void MapMode::_CreateMinimap()
{
    if(_minimap) {
        delete _minimap;
        _minimap = NULL;
    }

    _minimap = new Minimap(_minimap_custom_image_file);
}

void MapMode::_UpdateExplore()
{
    // First go to menu mode if the user requested it
    if(InputManager->MenuPress()) {
        MenuMode *MM = new MenuMode();
        ModeManager->Push(MM);
        return;
    }

    // Update the running state of the camera object. Check if the character is running and if so,
    // update the stamina value if the operation is permitted
    _camera->is_running = false;
    if(_camera->moved_position && !_running_disabled && InputManager->CancelState() &&
            (InputManager->UpState() || InputManager->DownState() || InputManager->LeftState() || InputManager->RightState())) {
        if(_unlimited_stamina) {
            _camera->is_running = true;
        } else if(_run_stamina > SystemManager->GetUpdateTime() * 2) {
            _run_stamina -= (SystemManager->GetUpdateTime() * 2);
            _camera->is_running = true;
        } else {
            _run_stamina = 0;
        }
    }
    // Regenerate the stamina at 1/2 the consumption rate
    else if(_run_stamina < 10000) {
        _run_stamina += SystemManager->GetUpdateTime();
        if(_run_stamina > 10000)
            _run_stamina = 10000;
    }

    // If the user requested a confirm event, check if there is a nearby object that the player may interact with
    // Interactions are currently limited to dialogue with sprites and opening of treasures
    if(InputManager->ConfirmPress()) {
        MapObject *obj = _object_supervisor->FindNearestInteractionObject(_camera);

        if(obj != NULL) {
            if(obj->GetType() == PHYSICAL_TYPE) {
                PhysicalObject *phs = reinterpret_cast<PhysicalObject *>(obj);

                if(!phs->GetEventIdWhenTalking().empty()) {
                    _camera->moving = false;
                    _camera->is_running = false;
                    if (!_event_supervisor->IsEventActive(phs->GetEventIdWhenTalking()))
                        _event_supervisor->StartEvent(phs->GetEventIdWhenTalking());
                    return;
                }
            }
            else if(obj->GetType() == SPRITE_TYPE) {
                MapSprite *sp = reinterpret_cast<MapSprite *>(obj);

                if(sp->HasAvailableDialogue()) {
                    _camera->moving = false;
                    _camera->is_running = false;
                    sp->InitiateDialogue();
                    return;
                }
            } else if(obj->GetType() == TREASURE_TYPE) {
                TreasureObject *treasure_object = reinterpret_cast<TreasureObject *>(obj);

                if(!treasure_object->GetTreasure()->IsTaken()) {
                    _camera->moving = false;
                    treasure_object->Open();
                }
            } else if(obj->GetType() == SAVE_TYPE) {
                // Make sure the character will be centered in the save point
                SaveMode *save_mode = new SaveMode(true, obj->GetXPosition(), obj->GetYPosition() - 1.0f);
                ModeManager->Push(save_mode, false, false);
            }
        }
    }

    // Detect movement input from the user
    if(InputManager->UpState() || InputManager->DownState() || InputManager->LeftState() || InputManager->RightState()) {
        _camera->moving = true;
    } else {
        _camera->moving = false;
    }

    // Determine the direction of movement. Priority of movement is given to: up, down, left, right.
    // In the case of diagonal movement, the direction that the sprite should face also needs to be deduced.
    if(_camera->moving == true) {
        if(InputManager->UpState()) {
            if(InputManager->LeftState())
                _camera->SetDirection(MOVING_NORTHWEST);
            else if(InputManager->RightState())
                _camera->SetDirection(MOVING_NORTHEAST);
            else
                _camera->SetDirection(NORTH);
        } else if(InputManager->DownState()) {
            if(InputManager->LeftState())
                _camera->SetDirection(MOVING_SOUTHWEST);
            else if(InputManager->RightState())
                _camera->SetDirection(MOVING_SOUTHEAST);
            else
                _camera->SetDirection(SOUTH);
        } else if(InputManager->LeftState()) {
            _camera->SetDirection(WEST);
        } else if(InputManager->RightState()) {
            _camera->SetDirection(EAST);
        }
    } // if (_camera->moving == true)
} // void MapMode::_UpdateExplore()



void MapMode::_UpdateMapFrame()
{
    // Determine the center position coordinates for the camera
    // Holds the final X, Y coordinates of the camera
    float camera_x = _camera->GetXPosition();
    float camera_y = _camera->GetYPosition();

    if(_camera_timer.IsRunning()) {
        camera_x += (1.0f - _camera_timer.PercentComplete()) * _delta_x;
        camera_y += (1.0f - _camera_timer.PercentComplete()) * _delta_y;
    }

    // Actual position of the view, either the camera sprite or a point on the camera movement path
    uint16 current_x = GetFloatInteger(camera_x);
    uint16 current_y = GetFloatInteger(camera_y);

    // NOTE: Would the map mode coordinate system be able to dynamically change, allow this to be recomputed,
    // and used as the multiple of the current camera tile offset, instead of PIXEL_LENGTH.
    //float pixel_length_x, pixel_length_y;
    //VideoManager->GetPixelSize(pixel_length_x, pixel_length_y);
    // NOTE: Until then, we'll hardcode the never-changing resulting value of 0.04f for performance purpose.
    const float PIXEL_LENGTH = 0.04f;

    // NOTE: The offset is corrected based on the map coord sys pixel size, to avoid glitches on tiles with transparent parts
    // and black edges. The size of the edge would have a variable size and look like vibrating when scrolling
    // without this fix.
    float current_offset_x = vt_utils::FloorToFloatMultiple(GetFloatFraction(camera_x), PIXEL_LENGTH);
    float current_offset_y = vt_utils::FloorToFloatMultiple(GetFloatFraction(camera_y), PIXEL_LENGTH);

    // Determine the draw coordinates of the top left corner using the camera's current position
    _map_frame.tile_x_offset = 1.0f - current_offset_x;
    if(IsOddNumber(current_x))
        _map_frame.tile_x_offset -= 1.0f;

    _map_frame.tile_y_offset = 2.0f - current_offset_y;
    if(IsOddNumber(current_y))
        _map_frame.tile_y_offset -= 1.0f;

    // The starting row and column of tiles to draw is determined by the map camera's position
    _map_frame.tile_x_start = (current_x / 2) - HALF_TILES_ON_X_AXIS;
    _map_frame.tile_y_start = (current_y / 2) - HALF_TILES_ON_Y_AXIS;

    _map_frame.screen_edges.top    = camera_y - HALF_SCREEN_GRID_Y_LENGTH;
    _map_frame.screen_edges.bottom = camera_y + HALF_SCREEN_GRID_Y_LENGTH;
    _map_frame.screen_edges.left   = camera_x - HALF_SCREEN_GRID_X_LENGTH;
    _map_frame.screen_edges.right  = camera_x + HALF_SCREEN_GRID_X_LENGTH;

    // Check for boundary conditions and re-adjust as necessary so we don't draw outside the map area

    // Usually the map centers on the camera's position, but when the camera becomes too close to
    // the edges of the map, we need to modify the drawing properties of the frame.

    // Reinit map corner check members
    _camera_x_in_map_corner = false;
    _camera_y_in_map_corner = false;

    // Camera exceeds the left boundary of the map
    if(_map_frame.tile_x_start < 0) {
        _map_frame.tile_x_start = 0;
        _map_frame.tile_x_offset = 1.0f;
        _map_frame.screen_edges.left = 0.0f;
        _map_frame.screen_edges.right = SCREEN_GRID_X_LENGTH;
        _camera_x_in_map_corner = true;
    }
    // Camera exceeds the right boundary of the map
    else if(_map_frame.tile_x_start + TILES_ON_X_AXIS >= _tile_supervisor->_num_tile_on_x_axis) {
        _map_frame.tile_x_start = static_cast<int16>(_tile_supervisor->_num_tile_on_x_axis - TILES_ON_X_AXIS);
        _map_frame.tile_x_offset = 1.0f;
        _map_frame.screen_edges.right = static_cast<float>(_object_supervisor->_num_grid_x_axis);
        _map_frame.screen_edges.left = _map_frame.screen_edges.right - SCREEN_GRID_X_LENGTH;
        _camera_x_in_map_corner = true;
    }

    // Camera exceeds the top boundary of the map
    if(_map_frame.tile_y_start < 0) {
        _map_frame.tile_y_start = 0;
        _map_frame.tile_y_offset = 2.0f;
        _map_frame.screen_edges.top = 0.0f;
        _map_frame.screen_edges.bottom = SCREEN_GRID_Y_LENGTH;
        _camera_y_in_map_corner = true;
    }
    // Camera exceeds the bottom boundary of the map
    else if(_map_frame.tile_y_start + TILES_ON_Y_AXIS >= _tile_supervisor->_num_tile_on_y_axis) {
        _map_frame.tile_y_start = static_cast<int16>(_tile_supervisor->_num_tile_on_y_axis - TILES_ON_Y_AXIS);
        _map_frame.tile_y_offset = 2.0f;
        _map_frame.screen_edges.bottom = static_cast<float>(_object_supervisor->_num_grid_y_axis);
        _map_frame.screen_edges.top = _map_frame.screen_edges.bottom - SCREEN_GRID_Y_LENGTH;
        _camera_y_in_map_corner = true;
    }

    // Determine the number of rows and columns of tiles that need to be drawn

    // When the tile images align perfectly with the screen, we can afford to draw one less row or column of tiles
    if(IsFloatInRange(_map_frame.tile_x_offset, 0.999f, 1.001f)) {
        _map_frame.num_draw_x_axis = TILES_ON_X_AXIS;
    } else {
        _map_frame.num_draw_x_axis = TILES_ON_X_AXIS + 1;
    }
    if(IsFloatInRange(_map_frame.tile_y_offset, 1.999f, 2.001f)) {
        _map_frame.num_draw_y_axis = TILES_ON_Y_AXIS;
    } else {
        _map_frame.num_draw_y_axis = TILES_ON_Y_AXIS + 1;
    }

    // Update parallax effects now that map corner members are up to date
    if(_camera_timer.IsRunning()) {
        // Inform the effect supervisor about camera movement.
        float duration = (float)_camera_timer.GetDuration();
        float time_elapsed = (float)SystemManager->GetUpdateTime();
        float x_parallax = !_camera_x_in_map_corner ?
                           _delta_x * time_elapsed / duration
                           / SCREEN_GRID_X_LENGTH * VIDEO_STANDARD_RES_WIDTH :
                           0.0f;
        float y_parallax = !_camera_y_in_map_corner ?
                           _delta_y * time_elapsed / duration
                           / SCREEN_GRID_Y_LENGTH * VIDEO_STANDARD_RES_HEIGHT :
                           0.0f;

        GetEffectSupervisor().AddParallax(x_parallax, y_parallax);
    }

    // Comment this out to print out map draw debugging info about once a second
//  static int loops = 0;
//  if (loops == 0) {
//      printf("--- MAP DRAW INFO ---\n");
//      printf("Rounded offsets:   [%f, %f]\n", current_offset_x, current_offset_y);
//      printf("Starting row, col: [%d, %d]\n", _map_frame.starting_row, _map_frame.starting_col);
//      printf("# draw rows, cols: [%d, %d]\n", _map_frame.num_draw_rows, _map_frame.num_draw_cols);
//      printf("Camera position:   [%f, %f]\n", camera_x, camera_y);
//      printf("Tile draw start:   [%f, %f]\n", _map_frame.tile_x_start, _map_frame.tile_y_start);
//      printf("Edges (T,B,L,R):   [%f, %f, %f, %f]\n", _map_frame.screen_edges.top, _map_frame.screen_edges.bottom,
//          _map_frame.screen_edges.left, _map_frame.screen_edges.right);
//  }
//
//  if (loops >= 60)
//      loops = 0;
//  else
//      ++loops;
} // void MapMode::_UpdateMapFrame()


void MapMode::_DrawDebugGrid()
{

    float tiles_x = TILES_ON_X_AXIS / (SCREEN_GRID_X_LENGTH / 2);
    float tiles_y = TILES_ON_Y_AXIS / (SCREEN_GRID_Y_LENGTH / 2);
    // Collision grid
    VideoManager->DrawGrid(_map_frame.tile_x_offset, _map_frame.tile_y_offset, tiles_x, tiles_y,
                           Color(0.0f, 0.0f, 0.5f, 1.0f));
    // Tile grid
    VideoManager->DrawGrid(_map_frame.tile_x_offset, _map_frame.tile_y_offset, tiles_x * 2, tiles_y * 2,
                           Color(0.5f, 0.0f, 0.0f, 1.0f));
}


void MapMode::_DrawMapLayers()
{
    VideoManager->SetCoordSys(0.0f, SCREEN_GRID_X_LENGTH, SCREEN_GRID_Y_LENGTH, 0.0f);

    _tile_supervisor->DrawLayers(&_map_frame, GROUND_LAYER);
    // Save points are engraved on the ground, and thus shouldn't be drawn after walls.
    _object_supervisor->DrawSavePoints();

    _object_supervisor->DrawFlatGroundObjects();
    _object_supervisor->DrawGroundObjects(false); // First draw pass of ground objects
    _object_supervisor->DrawPassObjects();
    _object_supervisor->DrawGroundObjects(true); // Second draw pass of ground objects

    _tile_supervisor->DrawLayers(&_map_frame, SKY_LAYER);

    _object_supervisor->DrawSkyObjects();

    if(VideoManager->DebugInfoOn()) {
        _object_supervisor->DrawCollisionArea(&_map_frame);
        _object_supervisor->_DrawMapZones();
        _DrawDebugGrid();
    }
} // void MapMode::_DrawMapLayers()



void MapMode::_DrawStaminaBar(const vt_video::Color &blending)
{
    // Don't draw anything when running is disabled.
    if (_running_disabled || blending.GetAlpha() == 0.0f)
        return;

    // It's the width of the stamina bar image to hide in pixels
    float fill_size = static_cast<float>(_run_stamina) / 10000.0f;
    fill_size = (1.0f - fill_size) * 200;

    VideoManager->PushState();
    VideoManager->SetStandardCoordSys();
    VideoManager->SetDrawFlags(VIDEO_X_LEFT, VIDEO_Y_BOTTOM, VIDEO_BLEND, 0);

    // Draw the background image
    VideoManager->Move(780, 747);
    _stamina_bar_background->Draw(blending);

    // Draw the stamina bar
    VideoManager->Move(801, 739);
    _stamina_bar->Draw(blending);

    if(_unlimited_stamina) {  // Draw the infinity symbol over the stamina bar
        VideoManager->Move(780, 747);
        _stamina_bar_infinite_overlay->Draw(blending);
    }
    else if(fill_size >= 2) {
        // Only do this if the part to hide is at least 2 pixels long
        VideoManager->Move(1001 - fill_size, 739);
        VideoManager->DrawRectangle(fill_size, 9, Color::black * blending);
    }
    VideoManager->PopState();
}

void MapMode::_DrawGUI()
{
    // Draw the introductory location name and graphic if necessary
    if(!_intro_timer.IsFinished()) {
        uint32 time = _intro_timer.GetTimeExpired();

        Color blend(1.0f, 1.0f, 1.0f, 1.0f);
        if(time < 1000) {  // Fade in
            blend.SetAlpha((static_cast<float>(time) / 1000.0f));
        } else if(time > 2000) { // Fade out
            blend.SetAlpha(1.0f - static_cast<float>(time - 2000) / 1000.0f);
        }

        // Don't draw the map location again, when it is the same as the last map.
        if(GlobalManager->ShouldDisplayHudNameOnMapIntro()) {
            VideoManager->PushState();
            VideoManager->SetStandardCoordSys();
            VideoManager->SetDrawFlags(VIDEO_X_CENTER, VIDEO_Y_CENTER, VIDEO_BLEND, 0);
            VideoManager->Move(512.0f, 100.0f);
            _map_image.Draw(blend);
            float shifting = (((float)time) - 2000.0f) / 100.0f;
            VideoManager->MoveRelative(0.0f + shifting, -80.0f);
            _map_hud_name.Draw(blend);
            VideoManager->PopState();
        }

        // Show the map subname when there is one
        VideoManager->PushState();
        VideoManager->SetStandardCoordSys();
        VideoManager->SetDrawFlags(VIDEO_X_CENTER, VIDEO_Y_CENTER, VIDEO_BLEND, 0);
        (GlobalManager->ShouldDisplayHudNameOnMapIntro() && !_map_hud_name.GetString().empty()) ?
        VideoManager->Move(512.0f, 170.0f) : VideoManager->Move(512.0f, 20.0f);
        _map_hud_subname.Draw(blend);
        VideoManager->PopState();

        // Draw the unlimited stamina bar with a fade out
        if(_unlimited_stamina) {
            _DrawStaminaBar(blend);
        } else if(time < 1000) {
            // Draw the normal bar fade in only (no fade out)
            _DrawStaminaBar(blend);
        } else {
            _DrawStaminaBar();
        }
    }

    // Draw the minimap
    if(_show_minimap && _minimap && (CurrentState() == STATE_EXPLORE)
            && GlobalManager->ShouldShowMinimap())
        _minimap->Draw();

    // Draw the stamina bar in the lower right corner
    if(!_unlimited_stamina && _intro_timer.IsFinished())
        _DrawStaminaBar(Color(1.0f, 1.0f, 1.0f, _gui_alpha));

    // Draw the debug info
    if(!VideoManager->DebugInfoOn())
        return;

    VideoManager->PushState();
    VideoManager->SetStandardCoordSys();
    VideoManager->SetDrawFlags(VIDEO_X_LEFT, VIDEO_Y_CENTER, VIDEO_BLEND, 0);
    VideoManager->Move(10.0f, 10.0f);
    _debug_camera_position.Draw();
    VideoManager->PopState();
} // void MapMode::_DrawGUI()

} // namespace vt_map

