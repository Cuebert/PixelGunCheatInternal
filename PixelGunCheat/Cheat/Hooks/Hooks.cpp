﻿#include "Hooks.h"

#include <codecvt>
#include <list>
#include <set>
#include <sstream>
#include <stdbool.h>

#include "MinHook.h"
#include "../Data/Weapons.h"
#include "../Internal/Functions.h"
#include "../Module/ModuleBase.h"
#include "../Module/Impl/ModuleAOEBullets.h"
#include "../Module/Impl/ModuleBetterScope.h"
#include "../Module/Impl/ModuleCriticals.h"
#include "../Module/Impl/ModuleDebuffer.h"
#include "../Module/Impl/ModuleFrostAura.h"
#include "../Module/Impl/ModuleFullAuto.h"
#include "../Module/Impl/ModuleHeal.h"
#include "../Module/Impl/ModuleInfiniteAmmo.h"
#include "../Module/Impl/ModuleInfiniteGemClaim.h"
#include "../Module/Impl/ModuleInstantCharge.h"
#include "../Module/Impl/ModuleInvisibility.h"
#include "../Module/Impl/ModuleRapidFire.h"
#include "../Module/Impl/ModuleReach.h"
#include "../Module/Impl/ModuleRecoil.h"
#include "../Module/Impl/ModuleScoreMultiplier.h"
#include "../Module/Impl/ModuleSpread.h"
#include "../Module/Impl/ModuleXRay.h"

#include "../Logger/Logger.h"
#include "../Module/Impl/ModuleFOVChanger.h"
#include "../Module/Impl/ModuleSpeed.h"

#include "../IL2CPPResolver/IL2CPP_Resolver.hpp"
#include "../Module/Impl/ModuleAddArmor.h"
#include "../Module/Impl/ModuleAddCurrency.h"
#include "../Module/Impl/ModuleAddPets.h"
#include "../Module/Impl/ModuleAimBot.h"
#include "../Module/Impl/ModuleAntiBarrier.h"
#include "../Module/Impl/ModuleAntiHeadshot.h"
#include "../Module/Impl/ModuleAntiImmortal.h"
#include "../Module/Impl/ModuleAntiKick.h"
#include "../Module/Impl/ModuleArrayList.h"
#include "../Module/Impl/ModuleDamageMultiplier.h"
#include "../Module/Impl/ModuleDoubleJump.h"
#include "../Module/Impl/ModuleESP.h"
#include "../Module/Impl/ModuleHeadshotMultiplier.h"
#include "../Module/Impl/ModulePriceModifier.h"
#include "../Module/Impl/ModuleRewardsMultiplier.h"
#include "../Module/Impl/ModuleImmortality.h"
#include "../Module/Impl/ModuleImmunity.h"
#include "../Module/Impl/ModuleInfiniteArmor.h"
#include "../Module/Impl/ModuleSeasonPass.h"
#include "../Module/Impl/ModuleTest.h"
#include "../Module/Impl/ModuleUnlockWeapons.h"
#include "../Module/Impl/ModuleUnlockWeapons.h"
#include "../Module/Impl/ModuleWeaponSpoofer.h"
#include "../Offsets/Offsets.h"

uintptr_t Hooks::GameBase;
uintptr_t Hooks::GameAssembly;
uintptr_t Hooks::UnityPlayer;

ModuleRapidFire* rapid_fire_module;
ModuleAntiKick* anti_kick;
ModuleSpeed* speed_module;
ModuleBase* infinite_gem_claim_module;
ModulePriceModifier* lottery_price_module;
ModuleBase* fast_levels_module;
ModuleRewardsMultiplier* rewards_multiplier_module;
ModuleESP* esp_module;
ModuleAimBot* aim_bot_module;
ModuleSeasonPass* season_pass_module;
ModuleBase* Hooks::fov_changer_module;
ModuleUnlockWeapons* unlock_weapons_module;
ModuleInfiniteAmmo* infinite_ammo_module;
ModuleDamageMultiplier* damage_multiplier_module;
ModuleAntiImmortal* anti_immortal_module;
ModuleImmortality* immortality_module;
ModuleAddArmor* add_armor_module;
ModuleAddPets* add_pets_module;
ModuleAddCurrency* add_currency_module;
ModuleWeaponSpoofer* weapon_spoofer_module;
std::list<ModuleBase*> player_move_c_modules = { };
std::list<ModuleBase*> player_move_c_others_modules = { };
std::list<ModuleBase*> player_fps_controller_sharp_modules = { };
std::list<ModuleBase*> weapon_sounds_modules = { };
std::list<ModuleBase*> weapon_sound_others_modules = { };
std::list<ModuleBase*> player_damageable_modules = { };
std::list<ModuleBase*> on_pre_render_modules = { };
std::list<ModuleBase*> Hooks::on_imgui_draw_modules = { };

uint64_t Hooks::tick = 0;
std::list<void*> working_player_list;
std::list<void*> Hooks::player_list;
void* Hooks::our_player;
void* Hooks::main_camera;
Unity::Vector3 zero = Unity::Vector3(0, 0, 0);
void* Hooks::aimed_pos = &zero;

// TODO: clean unused imports in all files

// Utility
void* Hooks::create_system_string(std::string string)
{
    Unity::System_String* ns = IL2CPP::String::New(string);
    return ns;
}

void* Hooks::create_system_string_w(std::wstring string)
{
    std::string s(string.begin(), string.end());
    Unity::System_String* ns = IL2CPP::String::New(s);
    return ns;
}

void Hooks::dump_item_records()
{
    Logger::log_debug("Dumping Records");
    auto dict = (Unity::il2cppDictionary<Unity::System_String*, void*>*)Functions::GetItemRecordDict();
    std::cout << &dict << std::endl;
    std::cout << dict->m_iCount << std::endl;
    for (int i = 0; i < dict->m_iCount; ++i)
    {
        Unity::System_String* key = dict->GetKeyByIndex(i);
        // wprintf(key->m_wString);
        // wprintf(L"\n");
        std::cout << key->ToString() << std::endl;
    }
    Logger::log_debug("Dumping Finished");
}

void Hooks::draw_all()
{
    if (esp_module == nullptr) return;
    esp_module->draw_all();
    if (aim_bot_module == nullptr) return;
    aim_bot_module->draw_all_aim();
}

void nuke_player_list()
{
    working_player_list.clear();
    Hooks::player_list.clear();
}

std::string clean_string(std::string string)
{
    std::vector<char> bytes(string.begin(), string.end());
    bytes.push_back('\0');
    std::list<char> chars;
    for (byte byte : bytes)
    {
        if (byte)
        {
            chars.push_back(byte);
        }
    }
    std::string clean(chars.begin(), chars.end());
    return clean;
}

std::string Hooks::get_player_name(void* player_move_c)
{
    if (player_move_c == nullptr) return "";
    void* nick_label = (void*)*(uint64_t*)((uint64_t)player_move_c + Offsets::nickLabel);
    void* name_ptr = Functions::TextMeshGetText(nick_label);
    if (name_ptr == nullptr) return "";
    std::string name = ((Unity::System_String*)name_ptr)->ToString();
    return clean_string(name);
}

void* Hooks::get_player_transform(void* player)
{
    return (void*)*(uint64_t*)((uint64_t)player + Offsets::myPlayerTransform);
}

bool Hooks::is_player_enemy(void* player)
{
    if (player == nullptr) return false;
    void* nick_label = (void*)*(uint64_t*)((uint64_t)player + Offsets::nickLabel);
    Unity::Color color = {0, 0,  0, 0};
    Functions::TextMeshGetColor(nick_label, &color);
    std::cout << std::to_string(color.r) << " | " << std::to_string(color.g) << " | " << std::to_string(color.b) << std::endl;
    return color.r == 1 && color.g == 0 && color.b == 0;
}

bool is_my_player_move_c(void* player_move_c)
{
    return Hooks::get_player_name(player_move_c) == "1111";
}

bool is_my_player_weapon_sounds(void* weapon_sounds)
{
    void* player_move_c = (void*)*(uint64_t*)((uint64_t)weapon_sounds + Offsets::weaponSoundsPlayerMoveC);
    if (player_move_c == nullptr) return false;
    return is_my_player_move_c(player_move_c);
}

std::string get_player_name_from_weapon_sounds(void* weapon_sounds)
{
    void* player_move_c = (void*)*(uint64_t*)((uint64_t)weapon_sounds + Offsets::weaponSoundsPlayerMoveC);
    if (player_move_c == nullptr) return "";
    return Hooks::get_player_name(player_move_c);
}


Unity::CCamera* find_main_camera()
{
    Unity::il2cppClass* camera_class = IL2CPP::Class::Find("UnityEngine.Camera");
    Unity::il2cppObject* camera_type = IL2CPP::Class::GetSystemType(camera_class);
    Unity::il2cppArray<Unity::CCamera*>* cameras = Unity::Object::FindObjectsOfType<Unity::CCamera>(camera_type);
    for (int i = 0; i < cameras->m_uMaxLength; i++)
    {
        Unity::CCamera* camera = cameras->At(i);
        std::string name = clean_string((*camera->GetName()).ToString());
        if (name == "ThirdPersonCamera(Clone)") return camera;
    }
    
    return Unity::Camera::GetMain();
}

// Hook Functions
inline void(__stdcall* weapon_sounds_original)(void* arg);
inline void __stdcall weapon_sounds_call(void* arg)
{
    if (unlock_weapons_module != nullptr)
    {
        ((ModuleBase*)unlock_weapons_module)->run(arg);
    }
    if (add_armor_module != nullptr)
    {
        ((ModuleBase*)add_armor_module)->run(arg);
    }
    if (add_pets_module != nullptr)
    {
        ((ModuleBase*)add_pets_module)->run(arg);
    }
    if (add_currency_module != nullptr)
    {
        ((ModuleBase*)add_currency_module)->run(arg);
    }
    
    if (is_my_player_weapon_sounds(arg))
    {
        if (Hooks::our_player != nullptr) ((ModuleBase*)aim_bot_module)->run(arg);
        
        for (ModuleBase* weapon_sounds_module : weapon_sounds_modules)
        {
            weapon_sounds_module->run(arg);
        }
    }
    else
    {
        for (ModuleBase* weapon_sounds_other_module : weapon_sound_others_modules)
        {
            weapon_sounds_other_module->run(arg);
        }
    }

    return weapon_sounds_original(arg);
}

inline void(__stdcall* player_move_c_original)(void* arg);
inline void __stdcall player_move_c(void* arg)
{
    bool my_player = is_my_player_move_c(arg);

    if (my_player)
    {
        if (Hooks::tick % 5 == 0)
        {
            Hooks::main_camera = find_main_camera();
            if (Hooks::main_camera == nullptr)
            {
                if (Hooks::main_camera == nullptr) return player_move_c_original(arg);
            }
            Hooks::our_player = arg;
        }

        Hooks::fov_changer_module->run(nullptr);
        
        for (ModuleBase* player_move_c_module : player_move_c_modules)
        {
            player_move_c_module->run(arg);
        }
    }
    else
    {
        // Other Players
        if (Hooks::main_camera == nullptr) return player_move_c_original(arg);
        Hooks::fov_changer_module->run(nullptr);
        esp_module->add_esp(arg);
        working_player_list.push_back(arg);

        for (auto player_move_c_others_module : player_move_c_others_modules)
        {
            player_move_c_others_module->run(arg);
        }
    }
    
    
    return player_move_c_original(arg);
}

inline void(__stdcall* player_move_c_fixed_original)(void* arg);
inline void __stdcall player_move_c_fixed(void* arg)
{
    bool my_player = is_my_player_move_c(arg);
    
    // Player Damageable
    void* player_damageable = (void*)*(uint64_t*)((uint64_t)arg + Offsets::playerMoveCPlayerDamageable);
    if (my_player)
    {
        for (ModuleBase* player_damageable_module : player_damageable_modules)
        {
            player_damageable_module->run(player_damageable);
        }
    }
    else
    {
        // Other Players
    }
    
    return player_move_c_fixed_original(arg);
}

inline bool(__stdcall* infinite_gem_claim_original)(void* arg);
inline bool __stdcall infinite_gem_claim(void* arg)
{
    if (infinite_gem_claim_module->is_enabled()) return true;
    
    return infinite_gem_claim_original(arg);
}

inline float(__stdcall* rapid_fire_original)(void* arg);
inline float __stdcall rapid_fire(void* arg)
{
    if (((ModuleBase*)rapid_fire_module)->is_enabled()) return rapid_fire_module->get_speed();

    return rapid_fire_original(arg);
}

inline float(__stdcall* speed_original)(void* arg);
inline float __stdcall speed(void* arg)
{
    if (((ModuleBase*)speed_module)->is_enabled()) return speed_module->get_amount();

    return speed_original(arg);
}

inline float(__stdcall* on_pre_render_original)(void* arg);
inline float __stdcall on_pre_render(void* arg)
{
    Hooks::tick++;
    
    Hooks::player_list = working_player_list;
    const std::list<void*> tl;
    working_player_list = tl;
    
    for (ModuleBase* on_pre_render_module : on_pre_render_modules)
    {
        on_pre_render_module->run(arg);
    }

    return on_pre_render_original(arg);
}

inline void (__stdcall* on_scene_unload_original)(void* arg);
inline void __stdcall on_scene_unload(void* arg)
{
    Hooks::main_camera = nullptr;
    nuke_player_list();
    // return on_scene_unload_original(arg);
    // Get Old Scene Name
    /*
    void* name_ptr = (void*)*(uint64_t*)((uint64_t)arg + 0x10);
    if (name_ptr == nullptr) return;
    std::string name = ((Unity::System_String*)name_ptr)->ToString();
    std::string clean = clean_string(name);
    std::cout << clean << std::endl;
    */
}

inline int (__stdcall* free_lottery_original)(void* arg);
inline int __stdcall free_lottery(void* arg)
{
    if (((ModuleBase*)lottery_price_module)->is_enabled())
    {
        return lottery_price_module->get_price();
    }
    
    return free_lottery_original(arg);
}

inline int (__stdcall* reward_multiplier_original)(void* arg);
inline int __stdcall reward_multiplier(void* arg)
{
    if (((ModuleBase*)rewards_multiplier_module)->is_enabled())
    {
        return rewards_multiplier_module->get_amount();
    }
    
    return reward_multiplier_original(arg);
}

inline bool (__stdcall* double_rewards_original)(void* arg);
inline bool __stdcall double_rewards(void* arg)
{
    if (((ModuleBase*)rewards_multiplier_module)->is_enabled())
    {
        return true;
    }
    
    return double_rewards_original(arg);
}

inline bool (__stdcall* season_pass_premium_original)(void* arg);
inline bool __stdcall season_pass_premium(void* arg)
{
    if (((ModuleBase*)season_pass_module)->is_enabled() && season_pass_module->spoof_premium())
    {
        return true;
    }
    
    return season_pass_premium_original(arg);
}

inline void (__stdcall* add_weapon_original)(void* arg, void* string, int source, bool bool1, bool bool2, void* class1, void* struct1);
inline void __stdcall add_weapon(void* arg, void* string, int source, bool bool1, bool bool2, void* class1, void* struct1)
{
    if (((ModuleBase*)unlock_weapons_module)->is_enabled())
    {
        Unity::System_String* sname = (Unity::System_String*)string;
        // Logger::log_info("Got Weapon: " + sname->ToString());
        // if (weapon_spoofer_module->all())
        if (false)
        {
            Logger::log_info("Adding All");
            for (int i = 0; i < weapon_spoofer_module->get_spoof_list().size(); i++)
            {
                // clear string
                sname->Clear();

                // Write String
                std::wstring nname = weapon_spoofer_module->get_spoof_list()[i];
                sname->m_iLength = (u_long)nname.length();
                for (u_long l = 0; l < nname.length(); l++)
                {
                    sname->m_wString[l] = nname[l];
                }

                // Logger::log_info("Changed To: " + sname->ToString());
                if (i % 50 == 0) Logger::log_info("Add Progress: " + std::to_string(i));
                
                // dev = 9999
                add_weapon_original(arg, string, weapon_spoofer_module->dev() ? 9999 : source, bool1, bool2, class1, struct1);
            }
            Logger::log_info("Done Adding");
            weapon_spoofer_module->lock();
            return;
        }
        else
        {
            /*
            if (weapon_spoofer_module->to_unlock().empty())
            {
                Logger::log_info("Invalid Weapon");
                return;
            }
            */
            // std::wstring nname = weapon_spoofer_module->to_unlock();
            std::wstring nname = unlock_weapons_module->get_current();
            std::string nnname(nname.begin(), nname.end());
            // Logger::log_info("Changing To: " + nnname);
            sname->Clear();
            sname->m_iLength = (u_long)nname.length();
            for (u_long l = 0; l < nname.length(); l++)
            {
                sname->m_wString[l] = nname[l];
            }
            
            // dev = 9999
            // add_weapon_original(arg, string, weapon_spoofer_module->dev() ? 9999 : source, bool1, bool2, class1, struct1);
            add_weapon_original(arg, string, 9999, bool1, bool2, class1, struct1);
            // Logger::log_info("Weapon Obtained");
            return;
        }
    }
    add_weapon_original(arg, string, source, bool1, bool2, class1, struct1);
}

inline int(__stdcall* ammo_in_clip_original)(void* arg);
inline int __stdcall ammo_in_clip(void* arg)
{
    if (((ModuleBase*)infinite_ammo_module)->is_enabled()) return 9999;

    return ammo_in_clip_original(arg);
}

inline int(__stdcall* ammo_original)(void* arg);
inline int __stdcall ammo(void* arg)
{
    if (((ModuleBase*)infinite_ammo_module)->is_enabled()) return 9999;

    return ammo_original(arg);
}

inline float(__stdcall* damage_multiplier_original)(void* arg);
inline float __stdcall damage_multiplier(void* arg)
{   
    if (((ModuleBase*)damage_multiplier_module)->is_enabled()) return damage_multiplier_module->amount();

    return damage_multiplier_original(arg);
}

inline bool(__stdcall* get_immortality_original)(void* arg);
inline bool __stdcall get_immortality(void* arg)
{
    if (((ModuleBase*)immortality_module)->is_enabled() && arg == Hooks::our_player) return true;
    if (((ModuleBase*)anti_immortal_module)->is_enabled() && arg != Hooks::our_player) return false;

    return get_immortality_original(arg);
}

// Static
void hook_function(uint64_t offset, LPVOID detour, void* original)
{
    if (MH_CreateHook((LPVOID*)(Hooks::GameAssembly + offset), detour, (LPVOID*)original) == MH_OK)
    {
        MH_EnableHook((LPVOID*)(Hooks::GameAssembly + offset));
    }
}

// Load
void Hooks::load()
{
    Logger::log_info("Creating hooks...");
    
    // Get Cool Base Offsets
    GameBase = (uintptr_t)GetModuleHandleA(NULL);
    GameAssembly = (uintptr_t)GetModuleHandleA("GameAssembly.dll");
    UnityPlayer = (uintptr_t)GetModuleHandleA("UnityPlayer.dll");
    Functions::init(GameBase, GameAssembly, UnityPlayer);

    // Cool IL2CPP Resolver
    IL2CPP::Initialize();
    Unity::Camera::Initialize();
    Unity::Transform::Initialize();
    
    // MinHook
    MH_Initialize();
    
    // Hook Functions Here
    hook_function(Offsets::WeaponSoundsUpdate, &weapon_sounds_call, &weapon_sounds_original);
    hook_function(Offsets::PlayerMoveCUpdate, &player_move_c, &player_move_c_original);
    hook_function(Offsets::InfiniteGemClaim, &infinite_gem_claim, &infinite_gem_claim_original);
    hook_function(Offsets::RapidFire, &rapid_fire, &rapid_fire_original);
    hook_function(Offsets::GetPlayerSpeed, &speed, &speed_original);
    hook_function(Offsets::OnPreRender, &on_pre_render, &on_pre_render_original);
    hook_function(Offsets::OnSceneUnload, &on_scene_unload, &on_scene_unload_original);
    hook_function(Offsets::PriceModifier, &free_lottery, &free_lottery_original);
    hook_function(Offsets::PlayerMoveCFixedUpdate, &player_move_c_fixed, &player_move_c_fixed_original);
    hook_function(Offsets::RewardMultiplier, &reward_multiplier, &reward_multiplier_original);
    hook_function(Offsets::DoubleRewards, &double_rewards, &double_rewards_original);
    hook_function(Offsets::PremiumPass, &season_pass_premium, &season_pass_premium_original);
    hook_function(Offsets::AddWeapon, &add_weapon, &add_weapon_original);
    hook_function(Offsets::GetAmmoInClip, &ammo_in_clip, &ammo_in_clip_original);
    hook_function(Offsets::GetAmmo, &ammo, &ammo_original);
    hook_function(Offsets::GetDamageMultiplier, &damage_multiplier, &damage_multiplier_original);
    hook_function(Offsets::PlayerGetImmortality, &get_immortality, &get_immortality_original);
    
    // Init Modules Here
    rapid_fire_module = new ModuleRapidFire();
    speed_module = new ModuleSpeed();
    infinite_gem_claim_module = (ModuleBase*) new ModuleInfiniteGemClaim();
    lottery_price_module = new ModulePriceModifier;
    rewards_multiplier_module = new ModuleRewardsMultiplier();
    season_pass_module = new ModuleSeasonPass();
    damage_multiplier_module = new ModuleDamageMultiplier();
    anti_immortal_module = new ModuleAntiImmortal();
    immortality_module = new ModuleImmortality();
    
    unlock_weapons_module = new ModuleUnlockWeapons();
    add_armor_module = new ModuleAddArmor();
    add_pets_module = new ModuleAddPets();
    add_currency_module = new ModuleAddCurrency();
    // weapon_spoofer_module = new ModuleWeaponSpoofer();

    esp_module = new ModuleESP();
    player_move_c_modules.push_back((ModuleBase*) esp_module);
    aim_bot_module = new ModuleAimBot();
    player_move_c_modules.push_back((ModuleBase*) new ModuleInvisibility());

    player_move_c_modules.push_back((ModuleBase*) new ModuleTest());

    
    /*
     does fucking nothing with these vals xddddd
    player_move_c_modules.push_back((ModuleBase*) new ModuleExtraDisplay());
    */
    
    on_imgui_draw_modules.push_back((ModuleBase*) new ModuleArrayList());
    
    weapon_sounds_modules.push_back((ModuleBase*) new ModuleCriticals());
    weapon_sounds_modules.push_back((ModuleBase*) new ModuleReach());
    weapon_sounds_modules.push_back((ModuleBase*) new ModuleRecoil());
    weapon_sounds_modules.push_back((ModuleBase*) new ModuleHeadshotMultiplier());
    weapon_sounds_modules.push_back((ModuleBase*) new ModuleSpread());
    weapon_sounds_modules.push_back((ModuleBase*) new ModuleDebuffer());
    weapon_sounds_modules.push_back((ModuleBase*) new ModuleAOEBullets());
    weapon_sounds_modules.push_back((ModuleBase*) new ModuleFrostAura());
    weapon_sounds_modules.push_back((ModuleBase*) new ModuleFullAuto());
    weapon_sounds_modules.push_back((ModuleBase*) new ModuleInstantCharge());
    weapon_sounds_modules.push_back((ModuleBase*) new ModuleBetterScope());
    weapon_sounds_modules.push_back((ModuleBase*) new ModuleScoreMultiplier());
    weapon_sounds_modules.push_back((ModuleBase*) new ModuleXRay());
    weapon_sounds_modules.push_back((ModuleBase*) new ModuleDoubleJump());
    // weapon_sounds_modules.push_back((ModuleBase*) new InfiniteArmor());
    weapon_sounds_modules.push_back((ModuleBase*) new ModuleAntiBarrier());
    weapon_sounds_modules.push_back((ModuleBase*) new ModuleImmunity());

    // Will wreak havoc on literally everyone, even other cheaters :D
    weapon_sounds_modules.push_back((ModuleBase*) new ModuleAntiHeadshot());

    infinite_ammo_module = new ModuleInfiniteAmmo();
    // player_damageable_modules.push_back((ModuleBase*) new ModuleInfiniteAmmo());
    // player_move_c_modules.push_back((ModuleBase*) new ModuleHeal());

    fov_changer_module = (ModuleBase*) new ModuleFOVChanger();
    on_pre_render_modules.push_back(fov_changer_module);

    // Post Module Load
    BKCImGuiHooker::modules_loaded = true;
}

void Hooks::unload()
{
    MH_DisableHook(MH_ALL_HOOKS);
}