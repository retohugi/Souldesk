#include "ConfigManager.h"
#include "SystemController.h"
#include <ArduinoJson.h>

ConfigManager::ConfigManager() {
    // Constructor
}

ConfigManager::ConfigManager(EventQueue& eventQueue) {
    _eventQueue = &eventQueue;
}

void ConfigManager::setEventQueue(EventQueue* queue) {
    _eventQueue = queue;
}

void ConfigManager::begin() {
    // Initialize preferences
    _preferences.begin(_namespace, false);
    _insightsPrefs.begin(_insightsNamespace, false);
    _cardPrefs.begin(_cardNamespace, false);
    
    // Check for multi-project migration on first boot
    // This ensures backward compatibility with existing installations
    if (!isMultiProjectMode() && (getTeamId() != NO_TEAM_ID || !getApiKey().isEmpty())) {
        Serial.println("ConfigManager: Existing single-project config detected, attempting migration...");
        migrateToMultiProject();
    }
    
    // Check initial API configuration state
    updateApiConfigurationState();
}

// Private helper to check and update API configuration state
void ConfigManager::updateApiConfigurationState() {
    // In multi-project mode, check if we have any valid projects
    if (isMultiProjectMode()) {
        std::vector<PostHogProject> projects = getProjects();
        bool hasValidProject = false;
        
        for (const auto& project : projects) {
            if (!project.teamId.isEmpty() && !project.apiKey.isEmpty()) {
                hasValidProject = true;
                break;
            }
        }
        
        if (hasValidProject) {
            SystemController::setApiState(ApiState::API_CONFIGURED);
        } else {
            SystemController::setApiState(ApiState::API_AWAITING_CONFIG);
        }
        return;
    }
    
    // Legacy single-project mode
    if (!_preferences.isKey(_teamIdKey) || getTeamId() == NO_TEAM_ID) {
        SystemController::setApiState(ApiState::API_AWAITING_CONFIG);
        return;
    }
    
    if (!_preferences.isKey(_apiKeyKey) || getApiKey().isEmpty()) {
        SystemController::setApiState(ApiState::API_AWAITING_CONFIG);
        return;
    }
    
    // Both team ID and API key are set
    SystemController::setApiState(ApiState::API_CONFIGURED);
}

// Helper method to commit changes to flash
void ConfigManager::commit() {
    _preferences.end();
    _insightsPrefs.end();
    _cardPrefs.end();
    
    _preferences.begin(_namespace, false);
    _insightsPrefs.begin(_insightsNamespace, false);
    _cardPrefs.begin(_cardNamespace, false);
}

bool ConfigManager::saveWiFiCredentials(const String& ssid, const String& password) {
    if (ssid.length() == 0 || ssid.length() > MAX_SSID_LENGTH) {
        return false;
    }

    if (password.length() > MAX_PASSWORD_LENGTH) {
        return false;
    }

    // Save credentials
    _preferences.putString(_ssidKey, ssid);
    _preferences.putString(_passwordKey, password);
    _preferences.putBool(_hasCredentialsKey, true);
    
    // Commit changes
    commit();
    
    // Publish event if event queue is available
    if (_eventQueue != nullptr) {
        _eventQueue->publishEvent(EventType::WIFI_CREDENTIALS_FOUND, "");
    }
    
    return true;
}

bool ConfigManager::getWiFiCredentials(String& ssid, String& password) {
    if (!hasWiFiCredentials()) {
        return false;
    }

    // Retrieve credentials
    ssid = _preferences.getString(_ssidKey, "");
    password = _preferences.getString(_passwordKey, "");
    
    return true;
}

void ConfigManager::clearWiFiCredentials() {
    _preferences.remove(_ssidKey);
    _preferences.remove(_passwordKey);
    _preferences.putBool(_hasCredentialsKey, false);
    
    // Commit changes
    commit();
    
    // Publish event if event queue is available
    if (_eventQueue != nullptr) {
        _eventQueue->publishEvent(EventType::NEED_WIFI_CREDENTIALS, "");
    }
}

bool ConfigManager::hasWiFiCredentials() {
    return _preferences.getBool(_hasCredentialsKey, false);
}

bool ConfigManager::checkWiFiCredentialsAndPublish() {
    bool hasCredentials = hasWiFiCredentials();
    
    if (_eventQueue != nullptr) {
        if (hasCredentials) {
            _eventQueue->publishEvent(EventType::WIFI_CREDENTIALS_FOUND, "");
        } else {
            _eventQueue->publishEvent(EventType::NEED_WIFI_CREDENTIALS, "");
        }
    }
    
    return hasCredentials;
}


void ConfigManager::setTeamId(int teamId) {
    _preferences.putInt(_teamIdKey, teamId);
    
    // Commit changes
    commit();
    
    updateApiConfigurationState();
}

int ConfigManager::getTeamId() {
    if (!_preferences.isKey(_teamIdKey)) {
        return NO_TEAM_ID;
    }
    return _preferences.getInt(_teamIdKey);
}

void ConfigManager::setRegion(String region) {
    _preferences.putString(_regionKey, region);
    
    // Commit changes
    commit();
    
    updateApiConfigurationState();
}

String ConfigManager::getRegion() {
    if (!_preferences.isKey(_regionKey)) {
        return "us";
    }
    return _preferences.getString(_regionKey);
}

void ConfigManager::clearTeamId() {
    _preferences.remove(_teamIdKey);
    
    // Commit changes
    commit();
    
    SystemController::setApiState(ApiState::API_AWAITING_CONFIG);
}

bool ConfigManager::setApiKey(const String& apiKey) {
    if (apiKey.length() == 0 || apiKey.length() > MAX_API_KEY_LENGTH) {
        SystemController::setApiState(ApiState::API_CONFIG_INVALID);
        return false;
    }

    _preferences.putString(_apiKeyKey, apiKey);
    
    // Commit changes
    commit();
    
    updateApiConfigurationState();
    return true;
}

String ConfigManager::getApiKey() {
    return _preferences.getString(_apiKeyKey, "");
}

void ConfigManager::clearApiKey() {
    _preferences.remove(_apiKeyKey);
    
    // Commit changes
    commit();
    
    SystemController::setApiState(ApiState::API_AWAITING_CONFIG);
}

std::vector<CardConfig> ConfigManager::getCardConfigs() {
    std::vector<CardConfig> configs;
    
    // Check if the key exists first to avoid error logs
    if (!_cardPrefs.isKey("config_list")) {
        return configs; // Return empty vector if no card config stored yet
    }
    
    // Get JSON string from preferences
    String jsonString = _cardPrefs.getString("config_list", "[]");
    
    // Parse JSON
    DynamicJsonDocument doc(2048);
    DeserializationError error = deserializeJson(doc, jsonString);
    
    if (error) {
        Serial.printf("Failed to parse card configs JSON: %s\n", error.c_str());
        return configs; // Return empty vector on parse error
    }
    
    // Convert JSON array to vector of CardConfig
    JsonArray array = doc.as<JsonArray>();
    for (JsonVariant v : array) {
        JsonObject obj = v.as<JsonObject>();
        if (obj.containsKey("type") && obj.containsKey("config") && obj.containsKey("order")) {
            CardConfig config;
            config.type = stringToCardType(obj["type"].as<String>());
            config.config = obj["config"].as<String>();
            config.order = obj["order"].as<int>();
            config.name = obj["name"].as<String>();
            config.projectId = obj["projectId"] | ""; // Default to empty if missing (backward compatibility)
            configs.push_back(config);
        }
    }
    
    return configs;
}

bool ConfigManager::saveCardConfigs(const std::vector<CardConfig>& configs) {
    // Create JSON document
    DynamicJsonDocument doc(2048);
    JsonArray array = doc.to<JsonArray>();
    
    // Convert vector to JSON array
    for (const CardConfig& config : configs) {
        JsonObject obj = array.createNestedObject();
        obj["type"] = cardTypeToString(config.type);
        obj["config"] = config.config;
        obj["order"] = config.order;
        obj["name"] = config.name;
        obj["projectId"] = config.projectId; // Include projectId in serialization
    }
    
    // Serialize to string
    String jsonString;
    if (serializeJson(doc, jsonString) == 0) {
        Serial.println("Failed to serialize card configs to JSON");
        return false;
    }
    
    // Save to preferences
    _cardPrefs.putString("config_list", jsonString);
    
    // Commit changes
    commit();
    
    // Publish event if event queue is available
    if (_eventQueue != nullptr) {
        _eventQueue->publishEvent(EventType::CARD_CONFIG_CHANGED, "");
    }
    
    return true;
}

// Multi-project implementation

String ConfigManager::generateProjectId() {
    return "proj_" + String(esp_random()) + "_" + String(millis());
}

bool ConfigManager::addProject(const PostHogProject& project) {
    std::vector<PostHogProject> projects = getProjects();
    
    // Check limits
    if (projects.size() >= MAX_PROJECTS) {
        Serial.println("ConfigManager: Maximum number of projects reached");
        return false;
    }
    
    // Validate project data
    if (project.name.length() == 0 || project.name.length() > MAX_PROJECT_NAME_LENGTH) {
        Serial.println("ConfigManager: Invalid project name length");
        return false;
    }
    
    if (project.teamId.length() == 0 || project.apiKey.length() == 0) {
        Serial.println("ConfigManager: Missing required project fields");
        return false;
    }
    
    // Check for duplicate names
    for (const auto& existingProject : projects) {
        if (existingProject.name == project.name) {
            Serial.println("ConfigManager: Project name already exists");
            return false;
        }
    }
    
    // Create project with auto-generated ID if not set
    PostHogProject newProject = project;
    if (newProject.id.isEmpty()) {
        newProject.id = generateProjectId();
    }
    
    projects.push_back(newProject);
    
    bool success = saveProjects(projects);
    if (success && _eventQueue) {
        _eventQueue->publishEvent(EventType::CARD_CONFIG_CHANGED, ""); // Reuse existing event
    }
    
    return success;
}

bool ConfigManager::updateProject(const String& projectId, const PostHogProject& project) {
    std::vector<PostHogProject> projects = getProjects();
    
    for (auto& p : projects) {
        if (p.id == projectId) {
            // Preserve ID, update other fields
            p.name = project.name;
            p.region = project.region;
            p.teamId = project.teamId;
            p.apiKey = project.apiKey;
            p.color = project.color;
            
            bool success = saveProjects(projects);
            if (success && _eventQueue) {
                _eventQueue->publishEvent(EventType::CARD_CONFIG_CHANGED, "");
            }
            return success;
        }
    }
    
    Serial.println("ConfigManager: Project not found for update");
    return false;
}

bool ConfigManager::removeProject(const String& projectId) {
    std::vector<PostHogProject> projects = getProjects();
    
    for (auto it = projects.begin(); it != projects.end(); ++it) {
        if (it->id == projectId) {
            projects.erase(it);
            
            bool success = saveProjects(projects);
            if (success && _eventQueue) {
                _eventQueue->publishEvent(EventType::CARD_CONFIG_CHANGED, "");
            }
            return success;
        }
    }
    
    Serial.println("ConfigManager: Project not found for removal");
    return false;
}

std::vector<PostHogProject> ConfigManager::getProjects() {
    std::vector<PostHogProject> projects;
    
    if (!isMultiProjectMode()) {
        // Return single project if not in multi-project mode
        if (getTeamId() != NO_TEAM_ID && !getApiKey().isEmpty()) {
            PostHogProject singleProject;
            singleProject.id = "legacy";
            singleProject.name = "Main Product";
            singleProject.region = getRegion();
            singleProject.teamId = String(getTeamId());
            singleProject.apiKey = getApiKey();
            singleProject.color = 0x1f77b4;
            projects.push_back(singleProject);
        }
        return projects;
    }
    
    // Get JSON string from preferences
    String jsonString = _preferences.getString(_projectsKey, "[]");
    
    // Parse JSON
    DynamicJsonDocument doc(MAX_PROJECTS_STORAGE);
    DeserializationError error = deserializeJson(doc, jsonString);
    
    if (error) {
        Serial.printf("ConfigManager: Failed to parse projects JSON: %s\n", error.c_str());
        return projects; // Return empty vector on parse error
    }
    
    // Convert JSON array to vector of PostHogProject
    JsonArray array = doc.as<JsonArray>();
    for (JsonVariant v : array) {
        JsonObject obj = v.as<JsonObject>();
        if (obj.containsKey("id") && obj.containsKey("name") && 
            obj.containsKey("teamId") && obj.containsKey("apiKey")) {
            
            PostHogProject project;
            project.id = obj["id"].as<String>();
            project.name = obj["name"].as<String>();
            project.region = obj["region"] | "us"; // Default to "us" if missing
            project.teamId = obj["teamId"].as<String>();
            project.apiKey = obj["apiKey"].as<String>();
            project.color = obj["color"] | 0x1f77b4; // Default color if missing
            
            projects.push_back(project);
        }
    }
    
    return projects;
}

PostHogProject ConfigManager::getProject(const String& projectId) {
    std::vector<PostHogProject> projects = getProjects();
    
    for (const auto& project : projects) {
        if (project.id == projectId) {
            return project;
        }
    }
    
    // Return empty project if not found
    return PostHogProject();
}

bool ConfigManager::hasProject(const String& projectId) {
    std::vector<PostHogProject> projects = getProjects();
    
    for (const auto& project : projects) {
        if (project.id == projectId) {
            return true;
        }
    }
    
    return false;
}

bool ConfigManager::migrateToMultiProject() {
    // Check if already migrated
    if (isMultiProjectMode()) {
        return true;
    }
    
    // Get current projects (which will create a legacy project if single-project config exists)
    std::vector<PostHogProject> projects = getProjects();
    
    if (projects.empty()) {
        // No existing config, just enable multi-project mode
        _preferences.putBool(_multiProjectModeKey, true);
        commit();
        return true;
    }
    
    // Update the legacy project ID to use a generated one
    projects[0].id = generateProjectId();
    
    // Save the projects array to persistent storage
    bool success = saveProjects(projects);
    
    if (success) {
        // Enable multi-project mode
        _preferences.putBool(_multiProjectModeKey, true);
        commit();
        
        Serial.println("ConfigManager: Successfully migrated to multi-project mode");
    }
    
    return success;
}

bool ConfigManager::isMultiProjectMode() {
    return _preferences.getBool(_multiProjectModeKey, false);
}

bool ConfigManager::saveProjects(const std::vector<PostHogProject>& projects) {
    // Create JSON document
    DynamicJsonDocument doc(MAX_PROJECTS_STORAGE);
    JsonArray array = doc.to<JsonArray>();
    
    // Convert vector to JSON array
    for (const PostHogProject& project : projects) {
        JsonObject obj = array.createNestedObject();
        obj["id"] = project.id;
        obj["name"] = project.name;
        obj["region"] = project.region;
        obj["teamId"] = project.teamId;
        obj["apiKey"] = project.apiKey;
        obj["color"] = project.color;
    }
    
    // Serialize to string
    String jsonString;
    if (serializeJson(doc, jsonString) == 0) {
        Serial.println("ConfigManager: Failed to serialize projects to JSON");
        return false;
    }
    
    // Check size limits
    if (jsonString.length() > MAX_PROJECTS_STORAGE) {
        Serial.printf("ConfigManager: Projects JSON too large: %d bytes (max %d)\n", 
                     jsonString.length(), MAX_PROJECTS_STORAGE);
        return false;
    }
    
    // Save to preferences
    _preferences.putString(_projectsKey, jsonString);
    commit();
    
    return true;
}