-- Set the namespace according to the map name.
local ns = {};
setmetatable(ns, {__index = _G});
layna_village_kalya_house_path_script = ns;
setfenv(1, ns);

-- The map name, subname and location image
map_name = "Mountain Village of Layna"
map_image_filename = "img/menus/locations/mountain_village.png"
map_subname = ""

-- The music file used as default background music on this map.
-- Other musics will have to handled through scripting.
music_filename = "mus/Caketown_1-OGA-mat-pablo.ogg"

-- c++ objects instances
local Map = {};
local ObjectManager = {};
local DialogueManager = {};
local EventManager = {};

local bronann = {};

-- the main map loading code
function Load(m)

    Map = m;
    ObjectManager = Map.object_supervisor;
    DialogueManager = Map.dialogue_supervisor;
    EventManager = Map.event_supervisor;

    Map.unlimited_stamina = true;

    _CreateCharacters();
    -- Set the camera focus on Bronann
    Map:SetCamera(bronann);

    _CreateNPCs();
    _CreateObjects();

    _CreateEvents();
    _CreateZones();

    -- Add clouds overlay
    Map:GetEffectSupervisor():EnableAmbientOverlay("img/ambient/clouds.png", 5.0, -5.0, true);

    _HandleCredits();
end

-- Handle the display of the new game credits
function _HandleCredits()
    -- Handle small credits triggering
    if (GlobalManager:DoesEventExist("game", "Start_Credits") == false) then
        -- Triggers the small credits display
        GlobalManager:SetEventValue("game", "Start_Credits", 1);
    end
    if (GlobalManager:DoesEventExist("game", "Credits_shown") == false) then
        Map:GetScriptSupervisor():AddScript("dat/credits/episode1_credits.lua");
    end
end

function Update()
    -- Check whether the character is in one of the zones
    _CheckZones();
end


-- Character creation
function _CreateCharacters()
    -- Default: From village center
    bronann = CreateSprite(Map, "Bronann", 61, 45);
    bronann:SetDirection(vt_map.MapMode.WEST);
    bronann:SetMovementSpeed(vt_map.MapMode.NORMAL_SPEED);

    -- set up the position according to the previous map
    if (GlobalManager:GetPreviousLocation() == "from_kalya_house_exterior") then
        bronann:SetPosition(43, 3);
        bronann:SetDirection(vt_map.MapMode.SOUTH);
    end
    if (GlobalManager:GetPreviousLocation() == "from grandma house") then
        bronann:SetPosition(12, 6);
        bronann:SetDirection(vt_map.MapMode.NORTH);
        AudioManager:PlaySound("snd/door_close.wav");
    end

    if (GlobalManager:GetPreviousLocation() == "from_kalya_house_small_passage") then
        bronann:SetPosition(6, 3);
        bronann:SetDirection(vt_map.MapMode.WEST);
    end

    Map:AddGroundObject(bronann);
end

local grandma = {};

function _ReloadGrandmaDialogue()
    local text = {}
    local dialogue = {}
    local event = {}

    grandma:ClearDialogueReferences();

    local chicken_left = 3;
    if (GlobalManager:GetEventValue("game", "layna_village_chicken1_found") == 1) then chicken_left = chicken_left - 1; end
    if (GlobalManager:GetEventValue("game", "layna_village_chicken2_found") == 1) then chicken_left = chicken_left - 1; end
    if (GlobalManager:GetEventValue("game", "layna_village_chicken3_found") == 1) then chicken_left = chicken_left - 1; end

    if (chicken_left > 0) then
        if (GlobalManager:GetEventValue("game", "layna_village_chicken_dialogue_done") == 1) then
            -- Tell Bronann to keep on searching
            if (chicken_left < 3) then
                dialogue = vt_map.SpriteDialogue("ep1_layna_village_granma_chicken_not_found1");
                if (chicken_left == 1) then
                    text = vt_system.Translate("My three chicken have flown away again this morning... Could you find the last one for me?");
                else
                    text = vt_system.Translate("My three chicken have flown away again this morning... Could you find the remaining ones for me?");
                end
                dialogue:AddLine(text, grandma);
                DialogueManager:AddDialogue(dialogue);
                grandma:AddDialogueReference(dialogue);
            else
                dialogue = vt_map.SpriteDialogue("ep1_layna_village_granma_chicken_not_found2");
                text = vt_system.Translate("My three chicken have flown away again this morning... Could you find them for me?");
                dialogue:AddLine(text, grandma);
                DialogueManager:AddDialogue(dialogue);
                grandma:AddDialogueReference(dialogue);
            end
        else
            -- Tell Bronann she can't find her chicken
            dialogue = vt_map.SpriteDialogue("ep1_layna_village_granma_chicken_not_found");
            text = vt_system.Translate("Ah! Bronann. Could you help your old grandma?");
            dialogue:AddLine(text, grandma);
            text = vt_system.Translate("Sure grandma.");
            dialogue:AddLineEmote(text, bronann, "interrogation");
            text = vt_system.Translate("My three chicken have flown away again... Could you find them for me?");
            dialogue:AddLine(text, grandma);
            text = vt_system.Translate("I'll see what I can do about it.");
            dialogue:AddLine(text, bronann);
            text = vt_system.Translate("Thank you, young one.");
            dialogue:AddLineEvent(text, grandma, "Chicken dialogue done", "");
            DialogueManager:AddDialogue(dialogue);
            grandma:AddDialogueReference(dialogue);

            event = vt_map.ScriptedEvent("Chicken dialogue done", "set_chicken_dialogue_done", "");
            EventManager:RegisterEvent(event);
        end
    elseif (GlobalManager:GetEventValue("game", "layna_village_chicken_reward_given") == 0) then
        -- Gives Bronann his reward
        dialogue = vt_map.SpriteDialogue();
        text = vt_system.Translate("Oh, they're all back, my Brave Hero...");
        dialogue:AddLine(text, grandma);
        text = vt_system.Translate("Nevermind that...");
        dialogue:AddLine(text, bronann);
        text = vt_system.Translate("Let me give you something as a reward.");
        dialogue:AddLineEvent(text, grandma, "", "Give Bronann the chicken reward");
        text = vt_system.Translate("Oh, thanks Grandma!");
        dialogue:AddLine(text, bronann);
        text = vt_system.Translate("You're very welcome, my dear one.");
        dialogue:AddLineEvent(text, grandma, "Chicken reward given", "");
        DialogueManager:AddDialogue(dialogue);
        grandma:AddDialogueReference(dialogue);

        event = vt_map.TreasureEvent("Give Bronann the chicken reward");
        event:AddObject(2, 2); -- 2 Medium healing potions
        EventManager:RegisterEvent(event);

        event = vt_map.ScriptedEvent("Chicken reward given", "set_chicken_reward_given", "");
        EventManager:RegisterEvent(event);
    else
        -- Default dialogue
        dialogue = vt_map.SpriteDialogue("ep1_layna_village_granma_default");
        text = vt_system.Translate("Ah! It's nice to see your dear young face around, Bronann. Come and chat with an old grandma.");
        dialogue:AddLine(text, grandma);
        text = vt_system.Translate("Er... Sorry grandma, I have to go! Maybe later?");
        dialogue:AddLineEmote(text, bronann, "exclamation");
        text = vt_system.Translate("Ah! You'll surely want to see the young lady living up there. Ah, youngins nowadays...");
        dialogue:AddLine(text, grandma);
        DialogueManager:AddDialogue(dialogue);
        grandma:AddDialogueReference(dialogue);
    end
end

function _CreateNPCs()
    local npc = {}
    local event = {}

    grandma = CreateNPCSprite(Map, "Old Woman1", vt_system.Translate("Brymir"), 7, 25);
    Map:AddGroundObject(grandma);
    grandma:SetDirection(vt_map.MapMode.SOUTH);

    _ReloadGrandmaDialogue();

    -- Adds the chicken when found.
    if (GlobalManager:GetEventValue("game", "layna_village_chicken1_found") == 1) then
        npc = CreateSprite(Map, "Chicken", 21, 36);
        Map:AddGroundObject(npc);
        event = vt_map.RandomMoveSpriteEvent("Chicken1 random move", npc, 1000, 1000);
        event:AddEventLinkAtEnd("Chicken1 random move", 4500); -- Loop on itself
        EventManager:RegisterEvent(event);
        EventManager:StartEvent("Chicken1 random move");
    end

    if (GlobalManager:GetEventValue("game", "layna_village_chicken2_found") == 1) then
        npc = CreateSprite(Map, "Chicken", 19, 34);
        Map:AddGroundObject(npc);
        event = vt_map.RandomMoveSpriteEvent("Chicken2 random move", npc, 1000, 1000);
        event:AddEventLinkAtEnd("Chicken2 random move", 4500); -- Loop on itself
        EventManager:RegisterEvent(event);
        EventManager:StartEvent("Chicken2 random move", 1200);
    end

    if (GlobalManager:GetEventValue("game", "layna_village_chicken3_found") == 1) then
        npc = CreateSprite(Map, "Chicken", 23, 33);
        Map:AddGroundObject(npc);
        event = vt_map.RandomMoveSpriteEvent("Chicken3 random move", npc, 1000, 1000);
        event:AddEventLinkAtEnd("Chicken3 random move", 4500); -- Loop on itself
        EventManager:RegisterEvent(event);
        EventManager:StartEvent("Chicken3 random move", 2100);
    end
end

function _CreateObjects()
    local object = {}

    -- Left tree "wall"
    object = CreateObject(Map, "Tree Big1", 0, 44);
    if (object ~= nil) then Map:AddGroundObject(object) end;
    object = CreateObject(Map, "Rock1", 0, 42);
    if (object ~= nil) then Map:AddGroundObject(object) end;
    object = CreateObject(Map, "Tree Small1", 0, 40);
    if (object ~= nil) then Map:AddGroundObject(object) end;
    object = CreateObject(Map, "Rock1", 0, 38);
    if (object ~= nil) then Map:AddGroundObject(object) end;
    object = CreateObject(Map, "Tree Small1", 0, 36);
    if (object ~= nil) then Map:AddGroundObject(object) end;
    object = CreateObject(Map, "Rock1", 0, 34);
    if (object ~= nil) then Map:AddGroundObject(object) end;
    object = CreateObject(Map, "Tree Small1", 0, 32);
    if (object ~= nil) then Map:AddGroundObject(object) end;
    object = CreateObject(Map, "Tree Big1", 0, 30);
    if (object ~= nil) then Map:AddGroundObject(object) end;
    object = CreateObject(Map, "Rock1", 0, 28);
    if (object ~= nil) then Map:AddGroundObject(object) end;
    object = CreateObject(Map, "Tree Small1", 0, 26);
    if (object ~= nil) then Map:AddGroundObject(object) end;
    object = CreateObject(Map, "Rock1", 0, 24);
    if (object ~= nil) then Map:AddGroundObject(object) end;
    object = CreateObject(Map, "Tree Big2", 0, 22);
    if (object ~= nil) then Map:AddGroundObject(object) end;
    object = CreateObject(Map, "Rock1", 0, 20);
    if (object ~= nil) then Map:AddGroundObject(object) end;
    object = CreateObject(Map, "Tree Small1", 0, 18);
    if (object ~= nil) then Map:AddGroundObject(object) end;
    object = CreateObject(Map, "Rock1", 0, 16);
    if (object ~= nil) then Map:AddGroundObject(object) end;
    object = CreateObject(Map, "Tree Small1", 0, 14);
    if (object ~= nil) then Map:AddGroundObject(object) end;
    object = CreateObject(Map, "Rock1", 0, 12);
    if (object ~= nil) then Map:AddGroundObject(object) end;
    object = CreateObject(Map, "Rock2", 0, 10);
    if (object ~= nil) then Map:AddGroundObject(object) end;
    object = CreateObject(Map, "Rock1", 0, 7);
    if (object ~= nil) then Map:AddGroundObject(object) end;
    object = CreateObject(Map, "Tree Small1", 0, 5);
    if (object ~= nil) then Map:AddGroundObject(object) end;
    object = CreateObject(Map, "Tree Big1", 0, 3);
    if (object ~= nil) then Map:AddGroundObject(object) end;
    object = CreateObject(Map, "Rock1", 15, 3);
    if (object ~= nil) then Map:AddGroundObject(object) end;
    object = CreateObject(Map, "Rock1", 15.5, 1.5);
    if (object ~= nil) then Map:AddGroundObject(object) end;

    -- Right tree "Wall"
    object = CreateObject(Map, "Rock2", 63, 38);
    if (object ~= nil) then Map:AddGroundObject(object) end;
    object = CreateObject(Map, "Tree Small1", 63, 36);
    if (object ~= nil) then Map:AddGroundObject(object) end;
    object = CreateObject(Map, "Rock1", 63, 34);
    if (object ~= nil) then Map:AddGroundObject(object) end;
    object = CreateObject(Map, "Tree Small1", 63, 32);
    if (object ~= nil) then Map:AddGroundObject(object) end;
    object = CreateObject(Map, "Rock1", 63, 30);
    if (object ~= nil) then Map:AddGroundObject(object) end;
    object = CreateObject(Map, "Tree Small2", 63.5, 28);
    if (object ~= nil) then Map:AddGroundObject(object) end;
    object = CreateObject(Map, "Rock1", 63, 26);
    if (object ~= nil) then Map:AddGroundObject(object) end;
    object = CreateObject(Map, "Tree Small1", 63, 24);
    if (object ~= nil) then Map:AddGroundObject(object) end;
    object = CreateObject(Map, "Rock1", 63, 22);
    if (object ~= nil) then Map:AddGroundObject(object) end;
    object = CreateObject(Map, "Tree Small1", 63, 20);
    if (object ~= nil) then Map:AddGroundObject(object) end;
    object = CreateObject(Map, "Rock1", 63, 18);
    if (object ~= nil) then Map:AddGroundObject(object) end;
    object = CreateObject(Map, "Tree Small2", 64.5, 16);
    if (object ~= nil) then Map:AddGroundObject(object) end;
    object = CreateObject(Map, "Rock1", 63, 12);
    if (object ~= nil) then Map:AddGroundObject(object) end;
    object = CreateObject(Map, "Tree Small2", 64, 10);
    if (object ~= nil) then Map:AddGroundObject(object) end;
    object = CreateObject(Map, "Tree Small1", 63, 8);
    if (object ~= nil) then Map:AddGroundObject(object) end;


    -- Secret shortcut hiders
    object = CreateObject(Map, "Tree Big1", 38, 40);
    if (object ~= nil) then Map:AddGroundObject(object) end;
    object = CreateObject(Map, "Tree Big1", 40, 42);
    if (object ~= nil) then Map:AddGroundObject(object) end;
    object = CreateObject(Map, "Tree Big1", 42, 40);
    if (object ~= nil) then Map:AddGroundObject(object) end;

    -- Cliff hiders
    object = CreateObject(Map, "Tree Small1", 14, 30);
    if (object ~= nil) then Map:AddGroundObject(object) end;
    object = CreateObject(Map, "Tree Small1", 55, 12);
    if (object ~= nil) then Map:AddGroundObject(object) end;

    -- Fence
    object = CreateObject(Map, "Fence1 l top left", 17, 32);
    if (object ~= nil) then Map:AddGroundObject(object) end;
    object = CreateObject(Map, "Fence1 horizontal", 19, 32);
    if (object ~= nil) then Map:AddGroundObject(object) end;
    object = CreateObject(Map, "Fence1 horizontal", 21, 32);
    if (object ~= nil) then Map:AddGroundObject(object) end;
    object = CreateObject(Map, "Fence1 horizontal", 23, 32);
    if (object ~= nil) then Map:AddGroundObject(object) end;
    object = CreateObject(Map, "Fence1 horizontal", 25, 32);
    if (object ~= nil) then Map:AddGroundObject(object) end;
    object = CreateObject(Map, "Fence1 horizontal", 27, 32);
    if (object ~= nil) then Map:AddGroundObject(object) end;
    object = CreateObject(Map, "Fence1 l top right", 29, 32);
    if (object ~= nil) then Map:AddGroundObject(object) end;

    object = CreateObject(Map, "Fence1 vertical", 17, 34);
    if (object ~= nil) then Map:AddGroundObject(object) end;
    object = CreateObject(Map, "Fence1 vertical", 17, 36);
    if (object ~= nil) then Map:AddGroundObject(object) end;

    object = CreateObject(Map, "Fence1 l bottom left", 17, 38);
    if (object ~= nil) then Map:AddGroundObject(object) end;
    object = CreateObject(Map, "Fence1 horizontal", 19, 38);
    if (object ~= nil) then Map:AddGroundObject(object) end;
    object = CreateObject(Map, "Fence1 horizontal", 21, 38);
    if (object ~= nil) then Map:AddGroundObject(object) end;
    object = CreateObject(Map, "Fence1 horizontal", 23, 38);
    if (object ~= nil) then Map:AddGroundObject(object) end;
    object = CreateObject(Map, "Fence1 horizontal", 25, 38);
    if (object ~= nil) then Map:AddGroundObject(object) end;
    object = CreateObject(Map, "Fence1 horizontal", 27, 38);
    if (object ~= nil) then Map:AddGroundObject(object) end;
    object = CreateObject(Map, "Fence1 l bottom right", 29, 38);
    if (object ~= nil) then Map:AddGroundObject(object) end;
    object = CreateObject(Map, "Fence1 vertical", 29, 34);
    if (object ~= nil) then Map:AddGroundObject(object) end;
    object = CreateObject(Map, "Fence1 vertical", 29, 36);
    if (object ~= nil) then Map:AddGroundObject(object) end;

    object = CreateObject(Map, "Bench1", 7, 24);
    if (object ~= nil) then Map:AddGroundObject(object) end;
    object = CreateObject(Map, "Barrel1", 20, 24);
    if (object ~= nil) then Map:AddGroundObject(object) end;

    -- Secret treasure chest
    local chest = CreateTreasure(Map, "kalya_house_path_chest", "Wood_Chest1", 8, 8);
    if (chest ~= nil) then
        chest:AddObject(1001, 1);
        Map:AddGroundObject(chest);
    end
end

-- Creates all events and sets up the entire event sequence chain
function _CreateEvents()
    local event = {};

    -- Triggered Events
    event = vt_map.MapTransitionEvent("to Village center", "dat/maps/layna_village/layna_village_center_map.lua",
                                       "dat/maps/layna_village/layna_village_center_script.lua", "from_kalya_house_path");
    EventManager:RegisterEvent(event);

    event = vt_map.MapTransitionEvent("to Kalya house exterior", "dat/maps/layna_village/layna_village_kalya_house_exterior_map.lua",
                                       "dat/maps/layna_village/layna_village_kalya_house_exterior_script.lua", "from_kalya_house_path");
    EventManager:RegisterEvent(event);

    event = vt_map.MapTransitionEvent("to grandma house", "dat/maps/layna_village/layna_village_kalya_house_path_small_house_map.lua",
                                       "dat/maps/layna_village/layna_village_kalya_house_path_small_house_script.lua", "from_kalya_house_path");
    EventManager:RegisterEvent(event);

    event = vt_map.MapTransitionEvent("to Kalya house small passage", "dat/maps/layna_village/layna_village_kalya_house_exterior_map.lua",
                                       "dat/maps/layna_village/layna_village_kalya_house_exterior_script.lua", "from_kalya_house_path_small_passage");
    EventManager:RegisterEvent(event);
end

-- zones
local village_center_zone = {};
local kalya_house_exterior_zone = {};
local grandma_house_entrance_zone = {};
local kalya_house_small_passage_zone = {};

function _CreateZones()
    -- N.B.: left, right, top, bottom
    village_center_zone = vt_map.CameraZone(62, 63, 42, 47);
    Map:AddZone(village_center_zone);

    kalya_house_exterior_zone = vt_map.CameraZone(26, 56, 0, 2);
    Map:AddZone(kalya_house_exterior_zone);

    grandma_house_entrance_zone = vt_map.CameraZone(11, 13, 7, 8);
    Map:AddZone(grandma_house_entrance_zone);

    kalya_house_small_passage_zone = vt_map.CameraZone(3, 8, 0, 1);
    Map:AddZone(kalya_house_small_passage_zone);
end

function _CheckZones()
    if (village_center_zone:IsCameraEntering() == true) then
        -- Stop the character as it may walk in diagonal, which is looking strange
        -- when entering
        bronann:SetMoving(false);
        EventManager:StartEvent("to Village center");
    end

    if (kalya_house_exterior_zone:IsCameraEntering() == true) then
        -- Stop the character as it may walk in diagonal, which is looking strange
        -- when entering
        bronann:SetMoving(false);
        EventManager:StartEvent("to Kalya house exterior");
    end

    if (grandma_house_entrance_zone:IsCameraEntering() == true) then
        -- Stop the character as it may walk in diagonal, which is looking strange
        -- when entering
        bronann:SetMoving(false);
        EventManager:StartEvent("to grandma house");
        AudioManager:PlaySound("snd/door_open2.wav");
    end

    if (kalya_house_small_passage_zone:IsCameraEntering() == true) then
        -- Stop the character as it may walk in diagonal, which is looking strange
        -- when entering
        bronann:SetMoving(false);
        EventManager:StartEvent("to Kalya house small passage");
    end
end


-- Map Custom functions
map_functions = {

    set_chicken_dialogue_done = function()
        GlobalManager:SetEventValue("game", "layna_village_chicken_dialogue_done", 1);
        _ReloadGrandmaDialogue();
    end,

    set_chicken_reward_given = function()
        GlobalManager:SetEventValue("game", "layna_village_chicken_reward_given", 1);
        _ReloadGrandmaDialogue();
    end
}
