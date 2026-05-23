#include <format>
#include <algorithm>

#include <zest/file/toml_utils.h>
#include <zest/logger/logger.h>

#pragma warning(disable : 4005)
#include <imgui.h>
#pragma warning(default : 4005)

#define DECLARE_SETTINGS
#include <zest/settings/settings.h>

namespace Zest
{

SettingsManager::SettingsManager()
{
    m_spRoot = std::make_shared<TreeNode>();
}

void SettingsManager::BuildTree() const
{
    m_spRoot->children.clear();
    m_spRoot->name = ">";

    for (const auto& [name, val] : m_sections)
    {
        std::shared_ptr<TreeNode> spNode = m_spRoot;
        auto dirs = string_split(name.ToString(), ".");
        for (auto& d : dirs)
        {
            if (spNode->children.find(d) == spNode->children.end())
            {
                spNode->children[d] = std::make_shared<TreeNode>();
            }
            spNode = spNode->children[d];
            spNode->name = d;
        }

        for (const auto& [id, setting] : val)
        {
            spNode->values.push_back(std::make_pair(name, id));
        }

        // Sort by name
        std::sort(spNode->values.begin(), spNode->values.end(), [&](auto lhs, auto rhs) {
            return lhs.second.ToString() < rhs.second.ToString();
        });
    }
}

const StringId& SettingsManager::GetCurrentTheme() const
{
    return m_currentTheme;
}

void SettingsManager::DrawTreeNode(const std::shared_ptr<TreeNode>& spNode) const
{
    // Display the tree node with ImGui::TreeNode
    if (ImGui::TreeNode(spNode->name.c_str()))
    {
        for (auto& [section, v] : spNode->values)
        {
            auto name = v.ToString();

            // Get the original value
            auto& val = m_sections[section][v];

            auto prefix = name.substr(0, 2);
            name = name.substr(2);

            if (prefix == "c_")
            {
                glm::vec4 v = val.ToVec4f();
                if (ImGui::ColorEdit4(name.c_str(), &v[0]))
                {
                    val.f4 = v;
                }
            }
            else if (prefix == "b_")
            {
                bool v = val.ToBool();
                if (ImGui::Checkbox(name.c_str(), &v))
                {
                    val.b = v;
                }
            }
            else if (prefix == "s_")
            {
                float f = 0.0f;
                switch (val.type)
                {
                    case SettingType::Float:
                        ImGui::DragFloat(name.c_str(), &val.f);
                        break;
                    case SettingType::Vec2f:
                        ImGui::DragFloat2(name.c_str(), &val.f);
                        break;
                    case SettingType::Vec2i:
                        ImGui::DragInt2(name.c_str(), &val.i2.x);
                        break;
                    case SettingType::Vec3f:
                        ImGui::DragFloat3(name.c_str(), &val.f);
                        break;
                    case SettingType::Vec4f:
                        ImGui::DragFloat4(name.c_str(), &val.f);
                        break;
                    default:
                        break;
                }
            }
        }

        // Children
        for (const auto& child : spNode->children)
        {
            DrawTreeNode(child.second);
        }

        ImGui::TreePop();
    }
}

void SettingsManager::DrawGUI(const std::string& name, bool* bOpen) const
{
    std::vector<Zest::StringId> sectionNames;

    if (ImGui::Begin(name.c_str(), bOpen))
    {
        // TODO: Only do this on occasion
        BuildTree();

        for (auto& [name, spChild] : m_spRoot->children)
        {
            DrawTreeNode(spChild);
        }
    }
    ImGui::End();
}

bool SettingsManager::Save(const std::filesystem::path& filePath) const
{
    toml::table tbl;
    for (auto& client : m_clients)
    {
        client.pfnSave(tbl);
    }

    for (const auto& [section, values] : m_sections)
    {
        auto path = string_split(section.ToString(), ".");

        toml::table* pParent = &tbl;
        for (auto& sub : path)
        {
            LOG(DBG, sub.c_str());
            auto itr = pParent->insert_or_assign(sub, toml::table{});
            pParent = itr.first->second.as_table();
        }

        for (const auto& [value_name, value] : values)
        {
            LOG(DBG, "Name: " << value_name.ToString() << ", Val:" << value.f4.x);
            switch (value.type)
            {
                case SettingType::Float:
                {
                    pParent->insert(value_name.ToString(), value.ToFloat());
                }
                break;
                case SettingType::Bool:
                {
                    pParent->insert(value_name.ToString(), value.ToBool());
                }
                break;
                case SettingType::Vec2f:
                {
                    toml_write_vec2(*pParent, value_name.ToString(), value.ToVec2f());
                }
                break;
                case SettingType::Vec3f:
                {
                    toml_write_vec3(*pParent, value_name.ToString(), value.ToVec3f());
                }
                break;
                case SettingType::Vec4f:
                {
                    toml_write_vec4(*pParent, value_name.ToString(), value.ToVec4f());
                }
                break;
                case SettingType::Vec2i:
                {
                    toml_write_vec2(*pParent, value_name.ToString(), value.ToVec2i());
                }
                break;
                case SettingType::Unknown:
                    break;
            }
        }
    }

    std::ofstream fs(filePath, std::ios_base::trunc);
    fs << tbl;
    return true;
}

bool SettingsManager::Load(const std::filesystem::path& path)
{
    toml::table tbl;
    try
    {
        tbl = toml::parse_file(path.string());

        std::function<void(std::string name, const toml::table&)> fnParse;
        fnParse = [&](auto tableName, auto tbl) {
            bool found = false;
            for (auto& client : m_clients)
            {
                if (client.pfnLoad(tableName, tbl))
                {
                    return;
                }
            }

            for (auto& [section, value] : tbl)
            {
                auto sectionId = StringId(std::string(section.str()));
                {
                    //auto valueId = StringId(std::string(value.str()));
                    if (value.is_table())
                    {
                        fnParse(tableName.empty() ? std::string(section.str()) : tableName + "." + std::string(section.str()), *value.as_table());
                    }
                    else if (value.is_array())
                    {
                        auto arr = value.as_array();
                        switch (arr->size())
                        {
                            case 2:
                                Set(tableName, sectionId, toml_read_vec2(value, glm::vec2(0.0f)));
                                break;
                            case 3:
                                Set(tableName, sectionId, toml_read_vec3(value, glm::vec3(0.0f)));
                                break;
                            case 4:
                                Set(tableName, sectionId, toml_read_vec4(value, glm::vec4(0.0f)));
                                break;
                        }
                    }
                    else if (value.is_floating_point())
                    {
                        Set(tableName, sectionId, (float)value.as_floating_point()->get());
                    }
                    else if (value.is_boolean())
                    {
                        Set(tableName, sectionId, (bool)value.as_boolean()->get());
                    }
                    else
                    {
                        LOG(WARNING, "Unknown table entry on reload: " << section.str());
                    }
                }
            }
        };
        fnParse(std::string(), tbl);
    }
    catch (const toml::parse_error&)
    {
        return false;
    }
    return true;
}

void SettingsManager::AddClient(SettingsClient client)
{
    m_clients.push_back(client);
}

} // namespace Zest
