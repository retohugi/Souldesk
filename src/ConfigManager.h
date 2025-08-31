#pragma once

#include <Arduino.h>
#include <Preferences.h>
#include <vector>
#include "EventQueue.h"
#include "config/CardConfig.h"

/**
/**
 * @brief PostHog project configuration structure
 */
struct PostHogProject {
    String id;           // Auto-generated UUID/hash (internal use only)
    String name;         // User-defined display name (primary identifier for users)
    String region;       // us/eu
    String teamId;       // PostHog team ID  
    String apiKey;       // API key
    uint32_t color;      // Theme color for UI distinction
    
    /**
     * @brief Default constructor
     */
    PostHogProject() : id(""), name(""), region("us"), teamId(""), apiKey(""), color(0x1f77b4) {}
    
    /**
     * @brief Constructor with parameters
     */
    PostHogProject(const String& n, const String& r, const String& t, const String& a, uint32_t c = 0x1f77b4) 
        : name(n), region(r), teamId(t), apiKey(a), color(c) {
        id = generateProjectId();
    }
    
    /**
     * @brief Generate a unique project ID
     */
    static String generateProjectId() {
        return "proj_" + String(esp_random()) + "_" + String(millis());
    }
};

/**
 * @class ConfigManager
 * @brief Manages persistent configuration storage for the device
 * 
 * Features:
 * - Secure storage of WiFi credentials
 * - PostHog API configuration (team ID and API key)
 * - Insight configuration management
 * - Event-based state change notifications
 * - Thread-safe operations
 * 
 * Uses ESP32's non-volatile storage (NVS) through Preferences library
 * with size limits enforced for all stored values.
 */
class ConfigManager {
public:
    static const int NO_TEAM_ID = -1;  // Sentinel value for no team ID

    /**
     * @brief Default constructor
     */
    ConfigManager();

    /**
     * @brief Constructor with event queue integration
     * @param eventQueue Reference to the event queue for state change notifications
     */
    ConfigManager(EventQueue& eventQueue);

    /**
     * @brief Initialize the configuration system
     */
    void begin();

    /**
     * @brief Set the event queue for state change notifications
     * @param queue Pointer to the event queue
     */
    void setEventQueue(EventQueue* queue);

    /**
     * @brief Store WiFi credentials in persistent storage
     * @param ssid Network SSID
     * @param password Network password
     * @return true if saved successfully, false otherwise
     */
    bool saveWiFiCredentials(const String& ssid, const String& password);

    /**
     * @brief Retrieve stored WiFi credentials
     * @param ssid Reference to store the SSID
     * @param password Reference to store the password
     * @return true if credentials exist and were retrieved, false otherwise
     */
    bool getWiFiCredentials(String& ssid, String& password);

    /**
     * @brief Remove stored WiFi credentials
     */
    void clearWiFiCredentials();

    /**
     * @brief Check if WiFi credentials are stored
     * @return true if credentials exist, false otherwise
     */
    bool hasWiFiCredentials();

    /**
     * @brief Check WiFi credentials and publish status event
     * @return true if credentials exist, false otherwise
     */
    bool checkWiFiCredentialsAndPublish();

    /**
     * @brief Store team identifier
     * @param teamId The team identifier to store
     */
    void setTeamId(int teamId);

    /**
     * @brief Store region of the project
     * @param region The region of the project
     */
    void setRegion(String region);

        /**
     * @brief Retrieve stored region of the project
     * @return The region of the project
     */
    String getRegion();

    /**
     * @brief Retrieve stored team identifier
     * @return The team ID or NO_TEAM_ID if not set
     */
    int getTeamId();

    /**
     * @brief Remove stored team identifier
     */
    void clearTeamId();

    /**
     * @brief Store API key
     * @param apiKey The API key to store
     * @return true if saved successfully, false otherwise
     */
    bool setApiKey(const String& apiKey);

    /**
     * @brief Retrieve stored API key
     * @return The API key or empty string if not set
     */
    String getApiKey();

    /**
     * @brief Remove stored API key
     */
    void clearApiKey();

    // Multi-project management methods
    
    /**
     * @brief Add a new PostHog project
     * @param project The project configuration to add
     * @return true if added successfully, false otherwise
     */
    bool addProject(const PostHogProject& project);

    /**
     * @brief Update an existing PostHog project
     * @param projectId The ID of the project to update
     * @param project The updated project configuration
     * @return true if updated successfully, false otherwise
     */
    bool updateProject(const String& projectId, const PostHogProject& project);

    /**
     * @brief Remove a PostHog project
     * @param projectId The ID of the project to remove
     * @return true if removed successfully, false otherwise
     */
    bool removeProject(const String& projectId);

    /**
     * @brief Get all configured PostHog projects
     * @return Vector of PostHogProject configurations
     */
    std::vector<PostHogProject> getProjects();

    /**
     * @brief Get a specific PostHog project by ID
     * @param projectId The ID of the project to retrieve
     * @return PostHogProject configuration, or empty project if not found
     */
    PostHogProject getProject(const String& projectId);

    /**
     * @brief Check if a project exists
     * @param projectId The ID of the project to check
     * @return true if project exists, false otherwise
     */
    bool hasProject(const String& projectId);

    /**
     * @brief Set the default project
     * @param projectId The ID of the project to set as default
     */
    void setDefaultProject(const String& projectId);

    /**
     * @brief Get the default project ID
     * @return The ID of the default project, or empty string if none set
     */
    String getDefaultProjectId();

    /**
     * @brief Get the default project configuration
     * @return PostHogProject configuration of default project
     */
    PostHogProject getDefaultProject();

    /**
     * @brief Migrate from single to multi-project configuration
     * @return true if migration successful, false otherwise
     */
    bool migrateToMultiProject();

    /**
     * @brief Check if system is using multi-project mode
     * @return true if multi-project mode is active
     */
    bool isMultiProjectMode();

    /**
     * @brief Generate a unique project ID
     * @return Generated project ID string
     */
    String generateProjectId();


    /**
     * @brief Get all configured cards from persistent storage
     * @return Vector of CardConfig objects representing enabled cards
     */
    std::vector<CardConfig> getCardConfigs();

    /**
     * @brief Save card configurations to persistent storage
     * @param configs Vector of CardConfig objects to save
     * @return true if saved successfully, false otherwise
     */
    bool saveCardConfigs(const std::vector<CardConfig>& configs);

private:
    
    /**
     * @brief Validates and updates the API configuration state
     * 
     * Checks if both team ID and API key are properly set in preferences.
     * Updates the system state via SystemController to one of:
     * - API_AWAITING_CONFIG: if either team ID or API key is missing
     * - API_CONFIGURED: if both team ID and API key are properly set
     */
    void updateApiConfigurationState();

    /**
     * @brief Ensures preferences changes are persisted to flash
     * 
     * Closes and reopens both preference namespaces to ensure changes
     * are written to flash storage. Required after any preference modifications
     * to ensure changes survive power cycles.
     */
    void commit();

    /**
     * @brief Save projects vector to persistent storage
     * @param projects Vector of PostHogProject configurations to save
     * @return true if saved successfully, false otherwise
     */
    bool saveProjects(const std::vector<PostHogProject>& projects);

    // Preferences instances for persistent storage
    Preferences _preferences;      ///< Main preferences storage instance
    Preferences _insightsPrefs;   ///< Separate storage for insight data
    Preferences _cardPrefs;       ///< Separate storage for card configurations

    // Namespace constants for preferences organization
    const char* _namespace = "wifi_config";        ///< Namespace for WiFi and general config
    const char* _insightsNamespace = "insights";   ///< Namespace for insight data
    const char* _cardNamespace = "cards";          ///< Namespace for card configurations

    // Storage keys for WiFi configuration
    const char* _ssidKey = "ssid";                ///< Key for stored WiFi SSID
    const char* _passwordKey = "password";         ///< Key for stored WiFi password
    const char* _hasCredentialsKey = "has_creds"; ///< Key for WiFi credentials presence flag

    // Storage keys for API configuration
    const char* _teamIdKey = "team_id";           ///< Key for stored team ID
    const char* _apiKeyKey = "api_key";           ///< Key for stored API key
    const char* _regionKey = "region";           ///< Key for stored region

    // Multi-project storage keys
    const char* _projectsKey = "projects";        ///< Key for projects JSON array
    const char* _multiProjectModeKey = "multi_mode"; ///< Key for multi-project mode flag

    // Storage size limits
    /** @brief Maximum length for WiFi SSID (per IEEE 802.11 spec) */
    static const size_t MAX_SSID_LENGTH = 32;
    /** @brief Maximum length for WiFi password (per WPA2 spec) */
    static const size_t MAX_PASSWORD_LENGTH = 64;
    /** @brief Maximum length for insight configuration data */
    static const size_t MAX_INSIGHT_LENGTH = 1024;
    /** @brief Maximum length for API key */
    static const size_t MAX_API_KEY_LENGTH = 64;
    /** @brief Maximum length for insight identifier */
    static const size_t MAX_INSIGHT_ID_LENGTH = 64;

    // Multi-project constraints (per spec)
    /** @brief Maximum number of projects */
    static const uint8_t MAX_PROJECTS = 8;
    /** @brief Maximum storage for all projects (4KB as per spec) */
    static const size_t MAX_PROJECTS_STORAGE = 4096;
    /** @brief Maximum length for project name */
    static const size_t MAX_PROJECT_NAME_LENGTH = 32;

    // Event system
    EventQueue* _eventQueue = nullptr;  ///< Optional event queue for state notifications
};