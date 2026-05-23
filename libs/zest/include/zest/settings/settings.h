#pragma once

#include <filesystem>
#include <functional>
#include <glm/glm.hpp>
#include <unordered_map>
#include <memory>

#include <tomlplusplus/toml.hpp>
#include <zest/string/string_utils.h>

namespace Zest
{

#define DECLARE_SETTING_VALUE(name) inline Zest::StringId name(#name);
#define DECLARE_SETTING_GROUP(group, name) inline Zest::StringId group(#name);

DECLARE_SETTING_GROUP(g_defaultTheme, themes.defaultTheme);
DECLARE_SETTING_GROUP(g_window, window);

// Grid
DECLARE_SETTING_VALUE(s_windowSize);
DECLARE_SETTING_VALUE(b_windowMaximized);
DECLARE_SETTING_VALUE(s_windowPosition);

enum class SettingType
{
    Unknown,
    Float,
    Vec2f,
    Vec3f,
    Vec4f,
    Vec2i,
    Bool
};

struct SettingValue
{
    SettingValue()
        : type(SettingType::Unknown)
        , f4(glm::vec4(0.0f))
    {
    }
    SettingValue(const glm::vec4& val)
        : type(SettingType::Vec4f)
        , f4(val)
    {
    }
    SettingValue(const glm::vec3& val)
        : type(SettingType::Vec3f)
        , f3(val)
    {
    }
    SettingValue(const glm::vec2& val)
        : type(SettingType::Vec2f)
        , f2(val)
    {
    }
    SettingValue(const glm::ivec2& val)
        : type(SettingType::Vec2i)
        , i2(val)
    {
    }
    SettingValue(const float& val)
        : type(SettingType::Float)
        , f(val)
    {
    }
    SettingValue(const bool& val)
        : type(SettingType::Bool)
        , b(val)
    {
    }

    glm::vec4 ToVec4f() const
    {
        if (type == SettingType::Unknown)
        {
            type = SettingType::Vec4f;
        }

        if (type == SettingType::Vec4f)
        {
            return f4;
        }
        return glm::vec4(f);
    }

    glm::vec2 ToVec2f() const
    {
        if (type == SettingType::Unknown)
        {
            type = SettingType::Vec2f;
        }

        switch (type)
        {
        case SettingType::Vec2f:
            return f2;
            break;
        case SettingType::Vec3f:
            return glm::vec2(f3);
            break;
        case SettingType::Vec4f:
            return glm::vec2(f4);
            break;
        case SettingType::Float:
            return glm::vec2(f);
            break;
        case SettingType::Unknown:
            break;
        default:
            break;
        }
        return glm::vec2(0.0f);
    }

    glm::ivec2 ToVec2i() const
    {
        if (type == SettingType::Unknown)
        {
            type = SettingType::Vec2i;
        }

        switch (type)
        {
        case SettingType::Vec2i:
            return glm::ivec2(i2);
            break;
        case SettingType::Vec2f:
            return glm::ivec2(f2);
            break;
        case SettingType::Vec3f:
            return glm::ivec2(f3);
            break;
        case SettingType::Vec4f:
            return glm::ivec2(f4);
            break;
        case SettingType::Float:
            return glm::ivec2(int(f));
            break;
        case SettingType::Unknown:
            break;
        default:
            break;
        }
        return glm::ivec2(0);
    }

    glm::vec3 ToVec3f() const
    {
        if (type == SettingType::Unknown)
        {
            type = SettingType::Vec3f;
        }

        switch (type)
        {
        case SettingType::Vec2f:
            return glm::vec3(f2.x, f2.y, 0.0f);
            break;
        case SettingType::Vec3f:
            return f3;
            break;
        case SettingType::Vec4f:
            return glm::vec3(f4);
            break;
        case SettingType::Float:
            return glm::vec3(f);
            break;
        case SettingType::Unknown:
            break;
        default:
            break;
        }
        return glm::vec3(0.0f);
    }

    float ToFloat() const
    {
        if (type == SettingType::Unknown)
        {
            type = SettingType::Float;
        }

        if (type == SettingType::Float)
        {
            return f;
        }
        return f4.x;
    }

    bool ToBool() const
    {
        if (type == SettingType::Unknown)
        {
            type = SettingType::Bool;
        }

        if (type == SettingType::Bool)
        {
            return b;
        }

        return f4.x > 0.0f ? true : false;
    }
    union
    {
        glm::vec4 f4 = glm::vec4(1.0f);
        glm::vec3 f3;
        glm::vec2 f2;
        glm::ivec2 i2;
        float f;
        bool b;
    };
    mutable SettingType type;
};

using SettingMap = std::unordered_map<StringId, SettingValue>;
using GroupMap = std::unordered_map<StringId, SettingMap>;
struct TreeNode
{
    std::string name;
    std::unordered_map<std::string, std::shared_ptr<TreeNode>> children;
    std::vector<std::pair<StringId, StringId>> values;
};

using fnLoadSettings = std::function<bool(const std::string& location, const toml::table&)>;
using fnSaveSettings = std::function<void(toml::table&)>;
struct SettingsClient
{
    fnLoadSettings pfnLoad; 
    fnSaveSettings pfnSave; 
};

class SettingsManager
{
public:
    SettingsManager();
    void AddClient(SettingsClient client);
    void DrawTreeNode(const std::shared_ptr<TreeNode>& spNode) const;
    void DrawGUI(const std::string& name, bool* pOpen) const;
    bool Save(const std::filesystem::path& path) const;
    bool Load(const std::filesystem::path& path);
    void Set(const StringId& section, const StringId& id, const SettingValue& value)
    {
        m_sections[section][id] = value;
    }

    const SettingValue& Get(const StringId& section, const StringId& id) const
    {
        auto& sectionMap = m_sections[section];
        return sectionMap[id];
    }

    float GetFloat(const StringId& section, const StringId& id) const
    {
        auto& sectionMap = m_sections[section];
        return sectionMap[id].ToFloat();
    }

    glm::vec2 GetVec2f(const StringId& section, const StringId& id) const
    {
        auto& sectionMap = m_sections[section];
        return sectionMap[id].ToVec2f();
    }

    glm::vec4 GetVec4f(const StringId& section, const StringId& id, const glm::vec4& def = glm::vec4(0.0f)) const
    {
        auto& sectionMap = m_sections[section];
        auto itr = sectionMap.find(id);
        if (itr != sectionMap.end())
        {
            return itr->second.ToVec4f();
        }
        sectionMap[id] = def;
        return def;
    }

    glm::ivec2 GetVec2i(const StringId& section, const StringId& id) const
    {
        auto& sectionMap = m_sections[section];
        return sectionMap[id].ToVec2i();
    }

    bool GetBool(const StringId& section, const StringId& id) const
    {
        auto& sectionMap = m_sections[section];
        return sectionMap[id].ToBool();
    }

    const StringId& GetCurrentTheme() const;
    SettingMap& GetSection(const StringId& section) const
    {
        return m_sections[section];
    }

    void BuildTree() const;

private:
    mutable GroupMap m_sections;
    StringId m_currentTheme = g_defaultTheme;
    mutable bool m_dirty = true;
    mutable std::shared_ptr<TreeNode> m_spRoot;
    std::vector<SettingsClient> m_clients;
};

class GlobalSettingsManager : public SettingsManager
{
public:
    static GlobalSettingsManager& Instance()
    {
        static GlobalSettingsManager setting;
        return setting;
    }
};

} // namespace Zest
