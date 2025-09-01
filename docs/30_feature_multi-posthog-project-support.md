# Multi-PostHog Project Support

This document outlines the specification for extending the DeskHog/Souldesk web portal and insight card system to support multiple PostHog projects, allowing users to configure and display insights from different PostHog instances and pro**Revised Card Configuration UI:**
```html
<div class="card-config">
    <div class="form-g3. Cross-project insight comparison cards
4. Project health monitoringp">
        <label for="card-project">PostHog Project</label>
        <select id="card-project" required>
            <option value="">Select Project</option>
            <!-- Populated from configured projects -->
        </select>
    </div>
    
    <div class="form-group">
        <label for="card-insight-id">Insight ID</label>
        <input type="text" id="card-insight-id" placeholder="e.g., AbCdEf123" required>
        <p class="tip">
            Copy the insight ID from your PostHog dashboard URL. 
            Example: https://app.posthog.com/insights/AbCdEf123
        </p>
    </div>
    
    <div class="form-group">
        <button type="button" onclick="testInsightConnection()">Test Insight</button>
        <span id="insight-test-result"></span>
    </div>
</div>
```

**Insight Testing (Embedded):**
```cpp
void CaptivePortal::handleTestInsight(AsyncWebServerRequest *request) {
    String projectId = request->getParam("projectId", true)->value();
    String insightId = request->getParam("insightId", true)->value();
    
    PostHogProject* project = _configManager.getProject(projectId);
    if (!project) {
        request->send(400, "application/json", "{\"success\":false,\"error\":\"Project not found\"}");
        return;
    }
    
    // Temporarily switch to test project and make one request
    String originalProject = _postHogClient.getCurrentProjectId();
    _postHogClient.setActiveProject(projectId);
    
    String response;
    bool success = _postHogClient.fetchInsight(insightId, response, true);
    
    // Restore original project
    _postHogClient.setActiveProject(originalProject);
    
    DynamicJsonDocument doc(256);
    doc["success"] = success;
    doc["error"] = success ? "" : "Unable to fetch insight";
    
    String responseStr;
    serializeJson(doc, responseStr);
    request->send(200, "application/json", responseStr);
}
```neously.

## Current Architecture Overview

### Existing Single-Project Configuration
- **Configuration**: Single project configuration via `ConfigManager` (team ID, API key, region)
- **Web Portal**: Simple form for one PostHog project (region, team ID, API key)
- **PostHog Client**: Single client ins### 12. Testing Strategy

#### 12.1 Hardware Testing
- Memory usage monitoring with multiple projects
- Network performance with project switching
- Flash wear testing for frequent config updates
- Power consumption impact assessment

#### 12.2 Functional Testing  
- Memory cleanup when projects are deleted
- Config migration from single to multi-project
- Visual project distinction on small display

#### 12.3 Edge Case Testing
- Maximum project limit handling
- Memory exhaustion scenarios
- Network failures during project operations
- Invalid project configuration handling

## 13. Specification Clarifications & Improvements fetching from one project
- **Insight Cards**: Cards tied to a single project's insights

### Current Components
- `ConfigManager`: Stores single project credentials
- `CaptivePortal`: Web interface for configuration
- `PostHogClient`: API client for single project
- Insight Cards: Display data from single project

## Proposed Multi-Project Architecture

### 1. Data Model Changes

#### 1.1 Project Configuration Structure
```cpp
struct PostHogProject {
    String id;           // Unique identifier (auto-generated)
    String name;         // Display name
    String region;       // us/eu
    String teamId;       // PostHog team ID  
    String apiKey;       // API key
    uint32_t color;      // Theme color for UI distinction
};
```

#### 1.2 Enhanced ConfigManager
**New Methods:**
```cpp
// Project management
bool addProject(const PostHogProject& project);
bool updateProject(const String& projectId, const PostHogProject& project);
bool removeProject(const String& projectId);
std::vector<PostHogProject> getProjects();
PostHogProject getProject(const String& projectId);
bool hasProject(const String& projectId);

// Migration from single to multi-project
bool migrateToMultiProject();
```

**Storage Format:**
- JSON array in NVRAM/SPIFFS
- Backward compatibility with existing single-project format
- Automatic migration on first boot with new firmware

### 2. Web Portal Enhancement

#### 2.1 Project Management UI
**New Portal Sections:**

**Projects Overview Panel:**
```html
<h2>PostHog Projects</h2>
<div class="config-section">
    <div id="projects-overview">
        <!-- Project cards with color coding -->
    </div>
    <button id="add-project-btn">Add New Project</button>
</div>
```

**Project Configuration Form:**
```html
<div id="project-config" style="display:none;">
    <h3 id="project-form-title">Add PostHog Project</h3>
    <form id="project-form" onsubmit="return saveProjectConfig()">
        <input type="hidden" id="project-edit-id" value="">
        <div class="form-group">
            <label for="project-name">Project Name</label>
            <input type="text" id="project-name" placeholder="Web Analytics" required>
        </div>
        <div class="form-group">
            <label for="project-region">Region</label>
            <select id="project-region" required>
                <option value="us">US</option>
                <option value="eu">EU</option>
            </select>
        </div>
        <div class="form-group">
            <label for="project-team-id">Team ID</label>
            <input type="text" id="project-team-id" placeholder="12345" required>
        </div>
        <div class="form-group">
            <label for="project-api-key">API Key</label>
            <input type="password" id="project-api-key" placeholder="phx_..." required>
        </div>
        <div class="form-group">
            <label for="project-color">Color</label>
            <input type="color" id="project-color" value="#1f77b4">
        </div>
        <div class="button-container">
            <button type="submit">Save Project</button>
            <button type="button" onclick="cancelProjectEdit()">Cancel</button>
        </div>
    </form>
</div>
```

#### 2.2 JavaScript Functions
**Project Management:**
```javascript
// Load projects from device
async function loadProjects() {
    try {
        const response = await fetch('/config/projects');
        const data = await response.json();
        renderProjectsOverview(data.projects);
    } catch (error) {
        console.error('Failed to load projects:', error);
    }
}

// Save project configuration to device
function saveProjectConfig() {
    const formData = new FormData(document.getElementById('project-form'));
    const projectData = Object.fromEntries(formData.entries());
    
    fetch('/config/projects', {
        method: 'POST',
        headers: {'Content-Type': 'application/json'},
        body: JSON.stringify(projectData)
    }).then(response => response.json())
      .then(data => {
          if (data.success) {
              loadProjects(); // Refresh display
              cancelProjectEdit();
          }
      });
    return false;
}
```

#### 2.3 CaptivePortal Handler Methods
**New Handler Methods:**
```cpp
// Serve projects configuration page
void handleProjectsConfig(AsyncWebServerRequest *request);

// Handle project save/update
void handleSaveProject(AsyncWebServerRequest *request);

// Handle project deletion  
void handleDeleteProject(AsyncWebServerRequest *request);

// Test project connection
void handleTestProject(AsyncWebServerRequest *request);
```

### 3. PostHog Client Refactoring

#### 3.1 Lightweight Multi-Project Architecture
**Memory-Conscious Approach:**
```cpp
class PostHogClient {
    std::vector<PostHogProject> _projects;
    String _activeProjectId;
    ConfigManager& _config;
    EventQueue& _eventQueue;
    
public:
    void loadProjectsFromConfig();
    bool setActiveProject(const String& projectId);
    PostHogProject* getCurrentProject();
    bool requestInsightData(const String& insightId, bool forceRefresh = false);
    
private:
    String buildInsightUrl(const String& insightId, const char* refreshMode) const;
    PostHogProject* findProject(const String& projectId);
};
```

**Rationale:**
- **Single client instance** - conserves memory on ESP32
- **Project switching** - change context instead of multiple clients
- **Lazy loading** - only load project data when needed
- **Memory efficient** - ~1KB overhead total vs ~2-4KB per client

#### 3.2 Enhanced Request Queue
```cpp
struct QueuedRequest {
    String project_id;     // Associated project for context switching
    String insight_id;     
    uint8_t retry_count;   
    bool force_refresh;    
    unsigned long timestamp;
};
```

**Request Processing:**
```cpp
void PostHogClient::processQueue() {
    if (request_queue.empty() || has_active_request) return;
    
    QueuedRequest request = request_queue.front();
    
    // Switch project context if needed
    if (_activeProjectId != request.project_id) {
        if (!setActiveProject(request.project_id)) {
            // Project not found or disabled, skip request
            request_queue.pop();
            return;
        }
    }
    
    // Process request with current project context
    String response;
    if (fetchInsight(request.insight_id, response, request.force_refresh)) {
        publishInsightDataEvent(request.project_id, request.insight_id, response);
        request_queue.pop();
    }
    // ... retry logic
}
```

### 4. Insight Card Extensions

#### 4.1 Project-Aware Card Configuration
```cpp
struct CardConfig {
    String cardId;
    String cardType;
    String projectId;      // NEW: Associated project
    String insightId;
    JsonDocument config;   // Card-specific configuration
    bool enabled;
    int position;          // Display order
};
```

#### 4.2 Card Display Enhancements
**Visual Project Identification:**
- Color-coded borders/headers based on project color
- Project name/icon in card header
- Project-specific error states

**Card Configuration UI:**
```html
<div class="card-config">
    <select id="card-project" required>
        <option value="">Select Project</option>
        <!-- Populated from projects list -->
    </select>
    <select id="card-insight" required>
        <option value="">Select Insight</option>
        <!-- Populated based on selected project -->
    </select>
</div>
```

### 5. Implementation Phases

#### Phase 1: Data Layer (Backend)
1. Extend `ConfigManager` with multi-project storage
2. Implement project CRUD operations
3. Add migration logic from single to multi-project
4. Update existing single-project APIs for backward compatibility

#### Phase 2: Web Portal (Frontend)
1. Add project management UI components
2. Implement project CRUD in JavaScript
3. Update card configuration to include project selection
4. Add project testing/validation

#### Phase 3: PostHog Client Refactoring
1. Implement multi-client architecture
2. Update request queue to handle multiple projects
3. Add project-specific error handling
4. Update insight fetching logic

#### Phase 4: Card System Integration
1. Update card configuration to include project context
2. Implement project-aware card rendering
3. Add visual project identification
4. Update card management UI

#### Phase 5: Advanced Features
1. Project-specific refresh intervals
2. Cross-project insight comparison cards
3. Project health monitoring
4. Bulk operations (enable/disable all cards from project)

### 6. User Experience Improvements

#### 6.1 Progressive Disclosure
- Start with single project mode for new users
- "Add Another Project" button to expand to multi-project mode
- Collapsible project sections for space efficiency

#### 6.2 Project Templates
- Quick setup for common PostHog configurations
- Import project settings from PostHog API
- Preset insight collections for different use cases

#### 6.3 Validation & Testing
- Real-time connection testing during project setup
- Insight preview during card configuration
- Project health indicators in overview

### 7. Migration Strategy

#### 7.1 Backward Compatibility
- Existing single-project configurations automatically become first project in multi-project array
- Existing cards remain functional
- No breaking changes to current API endpoints

#### 7.2 Data Migration
```cpp
bool ConfigManager::migrateToMultiProject() {
    // Check if already migrated
    if (isMultiProjectMode()) return true;
    
    // Get current projects (creates legacy project if single-project config exists)
    std::vector<PostHogProject> projects = getProjects();
    
    if (projects.empty()) {
        // No existing config, just enable multi-project mode
        _preferences.putBool(_multiProjectModeKey, true);
        return true;
    }
    
    // Update the legacy project ID to use a generated one
    projects[0].id = generateProjectId();
    
    // Save projects and enable multi-project mode
    bool success = saveProjects(projects);
    if (success) {
        _preferences.putBool(_multiProjectModeKey, true);
    }
    return success;
}
```

### 8. Configuration Examples

#### 8.1 Multi-Project Setup
```json
{
    "projects": [
        {
            "id": "webapp_analytics",
            "name": "Web Application",
            "region": "us",
            "teamId": "12345",
            "apiKey": "phx_...",
            "enabled": true,
            "color": "#1f77b4"
        },
        {
            "id": "mobile_analytics", 
            "name": "Mobile App",
            "region": "us",
            "teamId": "67890",
            "apiKey": "phx_...",
            "enabled": true,
            "color": "#ff7f0e"
        },
        {
            "id": "api_monitoring",
            "name": "API Service",
            "region": "eu", 
            "teamId": "54321",
            "apiKey": "phx_...",
            "enabled": true,
            "color": "#2ca02c"
        }
    ],
    "cards": [
        {
            "cardId": "card1",
            "projectId": "webapp_analytics",
            "insightId": "weekly_users",
            "cardType": "trend",
            "position": 0
        },
        {
            "cardId": "card2", 
            "projectId": "mobile_analytics",
            "insightId": "daily_sessions",
            "cardType": "number",
            "position": 1
        },
        {
            "cardId": "card3",
            "projectId": "api_monitoring", 
            "insightId": "error_rate",
            "cardType": "trend",
            "position": 2
        }
    ]
}
```

### 9. Web Portal Implementation Details

#### 9.1 CaptivePortal Route Handlers

**Project Configuration Handler:**
```cpp
void CaptivePortal::handleProjectsConfig(AsyncWebServerRequest *request) {
    DynamicJsonDocument doc(2048); // Size for multiple projects
    JsonArray projectsArray = doc.createNestedArray("projects");
    
    std::vector<PostHogProject> projects = _configManager.getProjects();
    for (const auto& project : projects) {
        JsonObject projectObj = projectsArray.createNestedObject();
        projectObj["id"] = project.id;
        projectObj["name"] = project.name;
        projectObj["region"] = project.region;
        projectObj["teamId"] = project.teamId;
        // Mask API key for security
        String maskedKey = project.apiKey.length() > 4 ? 
            "****..." + project.apiKey.substring(project.apiKey.length() - 4) : "";
        projectObj["apiKey"] = maskedKey;
        projectObj["enabled"] = project.enabled;
        projectObj["color"] = String(project.color, HEX);
    }
    
    String response;
    serializeJson(doc, response);
    request->send(200, "application/json", response);
}
```

**Project Save Handler:**
```cpp
void CaptivePortal::handleSaveProject(AsyncWebServerRequest *request) {
    if (request->hasParam("name", true) && 
        request->hasParam("teamId", true) && 
        request->hasParam("apiKey", true)) {
        
        PostHogProject project;
        project.id = _configManager.generateProjectId();
        project.name = request->getParam("name", true)->value();
        project.region = request->getParam("region", true)->value();
        project.teamId = request->getParam("teamId", true)->value();
        project.apiKey = request->getParam("apiKey", true)->value();
        project.enabled = request->hasParam("enabled", true);
        
        // Parse color from hex string
        String colorStr = request->getParam("color", true)->value();
        project.color = strtoul(colorStr.substring(1).c_str(), NULL, 16);
        
        bool success = _configManager.addProject(project);
        
        DynamicJsonDocument response(256);
        response["success"] = success;
        response["message"] = success ? "Project saved" : "Failed to save project";
        
        String responseStr;
        serializeJson(response, responseStr);
        request->send(200, "application/json", responseStr);
    } else {
        request->send(400, "application/json", "{\"error\":\"Missing required fields\"}");
    }
}
```

### 10. Memory Constraints & Resource Management

#### 10.1 ESP32 Memory Considerations
**Memory Budget:**
- **Total SRAM**: ~320KB available
- **Current Usage**: ~200KB for display, WiFi, LVGL
- **Available for Multi-Project**: ~50-80KB maximum
- **Per Project Overhead**: ~500 bytes config + ~1KB cached insights

**Resource Limits:**
```cpp
static const uint8_t MAX_PROJECTS = 8;          // Maximum projects
static const uint8_t MAX_ACTIVE_PROJECTS = 3;   // Concurrent active projects  
static const size_t MAX_CACHED_INSIGHTS = 10;   // Per project insight cache
static const size_t INSIGHT_CACHE_SIZE = 1024;  // Bytes per cached insight
```

#### 10.2 Memory-Efficient Implementation
**Project Storage:**
```cpp
class ConfigManager {
private:
    static const size_t MAX_PROJECTS_STORAGE = 4096; // 4KB total for all projects
    PostHogProject _projects[MAX_PROJECTS];
    uint8_t _projectCount;
    
public:
    bool addProject(const PostHogProject& project) {
        if (_projectCount >= MAX_PROJECTS) return false;
        if (calculateProjectsSize() + sizeof(PostHogProject) > MAX_PROJECTS_STORAGE) return false;
        
        _projects[_projectCount++] = project;
        return saveProjects();
    }
};
```

**Smart Caching:**
```cpp
class PostHogClient {
private:
    struct CachedInsight {
        String projectId;
        String insightId; 
        String data;
        unsigned long timestamp;
    };
    
    CachedInsight _cache[MAX_CACHED_INSIGHTS];
    uint8_t _cacheCount;
    
    void evictOldestCache() {
        // Remove oldest cached insight when memory is full
        unsigned long oldest = ULONG_MAX;
        uint8_t oldestIndex = 0;
        
        for (uint8_t i = 0; i < _cacheCount; i++) {
            if (_cache[i].timestamp < oldest) {
                oldest = _cache[i].timestamp;
                oldestIndex = i;
            }
        }
        
        // Shift array to remove oldest entry
        for (uint8_t i = oldestIndex; i < _cacheCount - 1; i++) {
            _cache[i] = _cache[i + 1];
        }
        _cacheCount--;
    }
};
```

### 11. Error Handling & Constraints

#### 11.1 Hardware Limitations
**ESP32 Constraints:**
- **Flash Storage**: Limited NVRAM space for project configurations  
- **Network**: Single WiFi connection, sequential HTTPS requests only
- **Processing**: Limited CPU for JSON parsing and display updates
- **Memory**: Strict limits on cached data and concurrent operations

**Graceful Degradation:**
- Disable projects when memory is low
- Prioritize first/primary project when resources are constrained  
- Cache eviction for least recently used insights
- Simplified UI when many projects are configured

#### 11.2 Connection & Error Handling
```cpp
bool PostHogClient::requestInsightData(const String& insightId, bool forceRefresh) {
    PostHogProject* project = getCurrentProject();
    if (!project || !project->enabled) {
        Serial.println("PostHog: No active project or project disabled");
        return false;
    }
    
    if (WiFi.status() != WL_CONNECTED) {
        publishErrorEvent(project->id, insightId, "WiFi not connected");
        return false;
    }
    
    // Add to queue with project context
    QueuedRequest request = {
        .project_id = project->id,
        .insight_id = insightId,
        .retry_count = 0,
        .force_refresh = forceRefresh,
        .timestamp = millis()
    };
    
    if (request_queue.size() >= MAX_QUEUE_SIZE) {
        Serial.println("PostHog: Request queue full, dropping oldest");
        request_queue.pop(); // Remove oldest request
    }
    
    request_queue.push(request);
    return true;
}
```

This specification provides a comprehensive roadmap for implementing multi-PostHog project support while maintaining backward compatibility and ensuring a smooth user experience.

## 12. Specification Clarifications & Improvements

## 13. Specification Clarifications & Improvements

### 13.1 Project Color Usage
**Where project colors are used:**
- **Card Headers**: Colored border or accent strip on insight cards to visually distinguish which project they belong to
- **Project Overview Cards**: Background or border color in the projects management panel
- **Status Indicators**: Connection status dots/icons in project color
- **Card Configuration UI**: Color-coded project selection dropdown
- **Device Display**: If multiple cards from different projects are shown simultaneously, color coding helps users identify data sources

**Implementation Example:**
```css
.card[data-project-id="webapp"] {
    border-left: 4px solid var(--project-webapp-color);
}
.card[data-project-id="mobile-app"] {
    border-left: 4px solid var(--project-mobile-color);
}
.card[data-project-id="api-service"] {
    border-left: 4px solid var(--project-api-color);
}
```

### 13.2 Project ID Design Decision
**Revised Approach - Auto-Generated IDs:**

```cpp
struct PostHogProject {
    String id;           // AUTO-GENERATED UUID/hash (internal use only)
    String name;         // USER-DEFINED display name (primary identifier for users)
    String region;       // us/eu
    String teamId;       // PostHog team ID  
    String apiKey;       // API key
    bool enabled;        // Active/inactive
    uint32_t color;      // Theme color for UI distinction
};
```

**Rationale:**
- **Internal ID**: Auto-generated UUID ensures uniqueness and prevents conflicts
- **User Name**: Human-readable identifier that users interact with
- **API Consistency**: Internal operations use stable IDs, UI uses names
- **Flexibility**: Users can rename projects without breaking internal references

**ID Generation:**
```cpp
String generateProjectId() {
    return "proj_" + String(esp_random()) + "_" + String(millis());
}
```

### 13.3 Simplified Project Management
**Rationale for Removing Enable/Disable:**

**User Experience Benefits:**
- **Cleaner Interface**: Users understand "Delete" better than "Disable"
- **Less Cognitive Load**: Fewer options = simpler decision making
- **Direct Actions**: Want to stop using a project? Just delete it
- **Easy Re-adding**: Projects can be quickly re-added when needed

**Technical Benefits:**
- **Reduced Complexity**: No state management for enabled/disabled projects
- **Simpler Codebase**: Less conditional logic throughout the system
- **Memory Efficiency**: Deleted projects free memory immediately
- **Fewer Bugs**: Less state to track = fewer edge cases

**Memory Management:**
- **Same Benefits**: Deleting projects achieves the same memory savings as disabling
- **Cleaner Storage**: No orphaned disabled project configurations
- **Immediate Cleanup**: Memory freed instantly on deletion

**Implementation:**
```cpp
struct PostHogProject {
    String id;           // AUTO-GENERATED UUID/hash (internal use only)
    String name;         // USER-DEFINED display name (primary identifier for users)
    String region;       // us/eu
    String teamId;       // PostHog team ID  
    String apiKey;       // API key
    uint32_t color;      // Theme color for UI distinction
    // Removed: bool enabled; - All projects are active when configured
};
```

### 13.4 Insight ID Configuration Approach
**Current Reality - Manual Insight ID Entry:**

You're absolutely correct. PostHog insight IDs are not easily discoverable via API and users typically copy them from the PostHog dashboard URL.

**Revised Card Configuration UI:**
```html
<div class="card-config">
    <div class="form-group">
        <label for="card-project">PostHog Project</label>
        <select id="card-project" required>
            <option value="">Select Project</option>
            <!-- Populated from configured projects -->
        </select>
    </div>
    
    <div class="form-group">
        <label for="card-insight-id">Insight ID</label>
        <input type="text" id="card-insight-id" placeholder="e.g., AbCdEf123" required>
        <p class="tip">
            Copy the insight ID from your PostHog dashboard URL. 
            Example: https://app.posthog.com/insights/AbCdEf123
            <br>
            <a href="#" onclick="showInsightIdHelp()">How to find insight ID?</a>
        </p>
    </div>
    
    <div class="form-group">
        <button type="button" onclick="testInsightConnection()">Test Insight</button>
        <span id="insight-test-result"></span>
    </div>
</div>
```

**Enhanced User Experience:**
- **Input Validation**: Check insight ID format (PostHog uses specific patterns)
- **Test Connection**: Validate insight ID works with selected project before saving
- **Help Modal**: Step-by-step guide to find insight IDs in PostHog dashboard
- **Recent IDs**: Cache recently used insight IDs for quick reuse
- **URL Parser**: Auto-extract insight ID if user pastes full PostHog URL

**Insight ID Validation:**
```javascript
function validateInsightId(insightId) {
    // PostHog insight IDs are typically 6-8 character alphanumeric
    const pattern = /^[A-Za-z0-9]{6,8}$/;
    return pattern.test(insightId);
}

async function testInsightConnection() {
    const projectId = document.getElementById('card-project').value;
    const insightId = document.getElementById('card-insight-id').value;
    
    if (!validateInsightId(insightId)) {
        showError("Invalid insight ID format");
        return;
    }
    
    try {
        const response = await fetch(`/api/projects/${projectId}/insights/${insightId}/test`);
        const result = await response.json();
        
        if (result.success) {
            showSuccess("✓ Insight accessible");
        } else {
            showError("✗ " + result.error);
        }
    } catch (error) {
        showError("Connection test failed");
    }
}
```

### 13.5 Updated Implementation Priorities

**Revised Architecture Focus:**
1. **Single PostHog client** with project context switching (not multiple clients)
2. **Memory-first design** - strict limits and efficient caching
3. **Simple web handlers** - direct form processing, no REST APIs
4. **Manual insight ID entry** with validation and testing tools
5. **Hardware-appropriate limits** - max 8 projects, 3 active simultaneously

**ESP32-Optimized Implementation:**
- Use stack allocation where possible instead of heap
- Minimize dynamic memory allocation
- Cache only essential data with LRU eviction
- Simple form-based web interface (no complex SPA)
- Direct NVRAM storage without complex database abstractions