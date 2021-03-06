-- Set the namespace according to the map name.
local ns = {};
setmetatable(ns, {__index = _G});
mt_elbrus_shrine6_script = ns;
setfenv(1, ns);

-- The map name, subname and location image
map_name = "Mt. Elbrus Shrine"
map_image_filename = "img/menus/locations/mountain_shrine.png"
map_subname = ""

-- The music file used as default background music on this map.
-- Other musics will have to handled through scripting.
music_filename = "mus/mountain_shrine.ogg"

-- c++ objects instances
local Map = {};
local ObjectManager = {};
local DialogueManager = {};
local EventManager = {};
local Script = {};

-- the main character handler
local hero = {};

-- Name of the main sprite. Used to reload the good one at the end of dialogue events.
local main_sprite_name = "";

-- the main map loading code
function Load(m)

    Map = m;
    ObjectManager = Map.object_supervisor;
    DialogueManager = Map.dialogue_supervisor;
    EventManager = Map.event_supervisor;
    Script = Map:GetScriptSupervisor();

    Map.unlimited_stamina = false;

    _CreateCharacters();
    _CreateObjects();

    -- Set the camera focus on hero
    Map:SetCamera(hero);
    -- This is a dungeon map, we'll use the front battle member sprite as default sprite.
    Map.object_supervisor:SetPartyMemberVisibleSprite(hero);

    _CreateEvents();
    _CreateZones();

    -- Add a mediumly dark overlay
    Map:GetEffectSupervisor():EnableAmbientOverlay("img/ambient/dark.png", 0.0, 0.0, false);

end

-- the map update function handles checks done on each game tick.
function Update()
    -- Check whether the character is in one of the zones
    _CheckZones();
end

-- Character creation
function _CreateCharacters()
    -- Default hero and position
    hero = CreateSprite(Map, "Bronann", 16, 11);
    hero:SetDirection(vt_map.MapMode.SOUTH);
    hero:SetMovementSpeed(vt_map.MapMode.NORMAL_SPEED);
    Map:AddGroundObject(hero);

    if (GlobalManager:GetPreviousLocation() == "from_shrine_first_floor_NW_left_door") then
        hero:SetPosition(16, 11);
        hero:SetDirection(vt_map.MapMode.SOUTH);
    elseif (GlobalManager:GetPreviousLocation() == "from_shrine_first_floor_NW_right_door") then
        hero:SetPosition(28, 11);
        hero:SetDirection(vt_map.MapMode.SOUTH);
    elseif (GlobalManager:GetPreviousLocation() == "from_shrine_first_floor_SE_top_door") then
        hero:SetPosition(42.5, 24);
        hero:SetDirection(vt_map.MapMode.WEST);
    elseif (GlobalManager:GetPreviousLocation() == "from_shrine_first_floor_SE_bottom_door") then
        hero:SetPosition(42.5, 34);
        hero:SetDirection(vt_map.MapMode.WEST);
    end
end

function _CreateObjects()
    local object = {}
    local npc = {}
    local dialogue = {}
    local text = {}
    local event = {}

    _add_flame(9.5, 7);
    _add_flame(33.5, 7);
end

function _add_flame(x, y)
    local object = vt_map.SoundObject("snd/campfire.ogg", x, y, 5.0);
    if (object ~= nil) then Map:AddAmbientSoundObject(object) end;
    object = vt_map.SoundObject("snd/campfire.ogg", x + 18.0, y, 5.0);
    if (object ~= nil) then Map:AddAmbientSoundObject(object) end;

    object = CreateObject(Map, "Flame1", x, y);
    Map:AddGroundObject(object);

    Map:AddHalo("img/misc/lights/torch_light_mask2.lua", x, y + 3.0,
        vt_video.Color(0.85, 0.32, 0.0, 0.6));
    Map:AddHalo("img/misc/lights/sun_flare_light_main.lua", x, y + 2.0,
        vt_video.Color(0.99, 1.0, 0.27, 0.1));
end

-- Creates all events and sets up the entire event sequence chain
function _CreateEvents()
    local event = {};
    local dialogue = {};
    local text = {};

    event = vt_map.MapTransitionEvent("to mountain shrine 1st floor NW room - left door", "dat/maps/mt_elbrus/mt_elbrus_shrine5_map.lua",
                                       "dat/maps/mt_elbrus/mt_elbrus_shrine5_script.lua", "from_shrine_first_floor_SW_left_door");
    EventManager:RegisterEvent(event);
    event = vt_map.MapTransitionEvent("to mountain shrine 1st floor NW room - right door", "dat/maps/mt_elbrus/mt_elbrus_shrine5_map.lua",
                                       "dat/maps/mt_elbrus/mt_elbrus_shrine5_script.lua", "from_shrine_first_floor_SW_right_door");
    EventManager:RegisterEvent(event);
    event = vt_map.MapTransitionEvent("to mountain shrine 1st floor SE room - top door", "dat/maps/mt_elbrus/mt_elbrus_shrine7_map.lua",
                                       "dat/maps/mt_elbrus/mt_elbrus_shrine7_script.lua", "from_shrine_first_floor_SW_top_door");
    EventManager:RegisterEvent(event);
    event = vt_map.MapTransitionEvent("to mountain shrine 1st floor SE room - bottom door", "dat/maps/mt_elbrus/mt_elbrus_shrine7_map.lua",
                                       "dat/maps/mt_elbrus/mt_elbrus_shrine7_script.lua", "from_shrine_first_floor_SW_bottom_door");
    EventManager:RegisterEvent(event);

end

-- zones
local to_shrine_NW_left_door_room_zone = {};
local to_shrine_NW_right_door_room_zone = {};
local to_shrine_SE_top_door_room_zone = {};
local to_shrine_SE_bottom_door_room_zone = {};

-- Create the different map zones triggering events
function _CreateZones()

    -- N.B.: left, right, top, bottom
    to_shrine_NW_left_door_room_zone = vt_map.CameraZone(14, 18, 7, 9);
    Map:AddZone(to_shrine_NW_left_door_room_zone);
    to_shrine_NW_right_door_room_zone = vt_map.CameraZone(26, 30, 7, 9);
    Map:AddZone(to_shrine_NW_right_door_room_zone);
    to_shrine_SE_top_door_room_zone = vt_map.CameraZone(45, 47, 22, 26);
    Map:AddZone(to_shrine_SE_top_door_room_zone);
    to_shrine_SE_bottom_door_room_zone = vt_map.CameraZone(45, 47, 32, 36);
    Map:AddZone(to_shrine_SE_bottom_door_room_zone);

end

-- Check whether the active camera has entered a zone. To be called within Update()
function _CheckZones()
    if (to_shrine_NW_left_door_room_zone:IsCameraEntering() == true) then
        hero:SetDirection(vt_map.MapMode.NORTH);
        EventManager:StartEvent("to mountain shrine 1st floor NW room - left door");
    elseif (to_shrine_NW_right_door_room_zone:IsCameraEntering() == true) then
        hero:SetDirection(vt_map.MapMode.NORTH);
        EventManager:StartEvent("to mountain shrine 1st floor NW room - right door");
    elseif (to_shrine_SE_top_door_room_zone:IsCameraEntering() == true) then
        hero:SetDirection(vt_map.MapMode.EAST);
        EventManager:StartEvent("to mountain shrine 1st floor SE room - top door");
    elseif (to_shrine_SE_bottom_door_room_zone:IsCameraEntering() == true) then
        hero:SetDirection(vt_map.MapMode.EAST);
        EventManager:StartEvent("to mountain shrine 1st floor SE room - bottom door");
    end

end

-- Map Custom functions
-- Used through scripted events
map_functions = {

}
