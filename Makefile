# ─────────────────────────────────────────────────────────────────────────────
# Makefile — OpenGLProject (macOS, mode hors-ligne)
#
# Usage:
#   make          → compile
#   make run      → compile + lance
#   make clean    → nettoie
# ─────────────────────────────────────────────────────────────────────────────

CXX      := g++
CXXFLAGS := -std=c++20 -O2 -g
DEFINES  := -DSTEAM_OFFLINE -D_GLAD_GL_H_

SRCDIR   := OpenGLProject
BINDIR   := .

# ── Chemins des dépendances ──────────────────────────────────────────────
BREW  := /opt/homebrew
INC   := -I$(SRCDIR) \
         -I$(SRCDIR)/dependencies \
         -I$(SRCDIR)/dependencies/glm \
         -I$(SRCDIR)/dependencies/assimp \
         -I$(BREW)/include \
         -I$(BREW)/include/freetype2

LIBS  := -L$(BREW)/lib \
         -lglfw -lassimp -lfreetype \
         -framework OpenGL \
         -framework OpenAL \
         -lpthread

# ── Fichiers sources ─────────────────────────────────────────────────────
CPPFILES := $(filter-out $(SRCDIR)/SteamManager.cpp, $(wildcard $(SRCDIR)/*.cpp))
CFILES   := $(SRCDIR)/glad.c
OBJFILES := $(CPPFILES:.cpp=.o) $(CFILES:.c=.o)

TARGET   := game

# ── Règles ────────────────────────────────────────────────────────────────

.PHONY: all clean run

all: $(TARGET)

$(TARGET): $(OBJFILES)
	$(CXX) $(CXXFLAGS) $^ $(LIBS) -o $@
	@echo "✅ Compilation terminée : ./$(TARGET)"

$(SRCDIR)/%.o: $(SRCDIR)/%.cpp
	$(CXX) $(CXXFLAGS) $(DEFINES) $(INC) -c $< -o $@

# glad.c : règle explicite (évite conflit pattern rule avec .cpp)
$(SRCDIR)/glad.o: $(SRCDIR)/glad.c
	$(CXX) $(CXXFLAGS) $(DEFINES) $(INC) -c $< -o $@

run: $(TARGET)
	@echo "🚀 Lancement..."
	./$(TARGET)

clean:
	rm -f $(OBJFILES) $(TARGET)
	@echo "🧹 Nettoyé."
