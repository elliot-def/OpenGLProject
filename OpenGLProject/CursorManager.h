#pragma once

#include <map>
#include <string>
#include <stdexcept>
#include <vector>

class Window;
typedef struct GLFWcursor GLFWcursor;

class CursorManager {
public:

	CursorManager(Window* window) : m_window(window) {
		setCursor("default");
	};
	~CursorManager() = default;

	void setCustomCursor(const char* cursorPath);
	void updateCursor();

	void loadWaitFrames();

	// Getters :
	std::string getCurrentCursorPath() const {
		return m_currentCursor.second;
	}

	std::string getCurrentCursorName() const {
		for (const auto& pair : m_cursorsAvailable) {
			if (pair.second == m_currentCursor.second) {
				return pair.first;
			}
		}
		throw std::invalid_argument("Cursor not found"); // Lance une exception si le curseur actuel n'est pas trouvé
	}

	// Setters :

	void setCursor(const std::string& cursorName) {
		// Le curseur est déjà celui demandé
		if (cursorName == m_currentCursor.first) { return; }

		auto it = m_cursorsAvailable.find(cursorName);
		if (it != m_cursorsAvailable.end()) {
			m_currentCursor = *it;
			setCustomCursor(it->second.c_str());
			return;
		}
		throw std::invalid_argument("Cursor name not found in available cursors");
	}

	void setSize(float size) {
		m_size = size;
	}

private:
	std::map<std::string, std::string> m_cursorsAvailable = {
		{"default",		"./res/textures/menu/cursor assets/cursor_none.png"},
		{"wait",		"./res/textures/menu/cursor assets/busy_circle_fade.png"},
		{"disabled",	"./res/textures/menu/cursor assets/disabled.png"},
		{"crosshair",	"./res/textures/menu/cursor assets/target_a.png.png"},
		{"text",		"./res/textures/menu/cursor assets/bracket_a_vertical.png"},
		{"grab",		"./res/textures/menu/cursor assets/hand_open.png"},
		{"grabbing",	"./res/textures/menu/cursor assets/hand_closed.png"}
	};

	std::pair<std::string, std::string> m_currentCursor = {"default", "./res/textures/menu/cursor assets/cursor_none.png"};
	GLFWcursor* m_GLFWcursor = nullptr;
	Window* m_window;

	// Variables pour l'animation :
	std::vector<GLFWcursor*> m_waitCursorFrames; // Stocke les pointeurs de chaque frame
	int m_currentFrameIndex = -1;

	float m_size = 0.5f;

};