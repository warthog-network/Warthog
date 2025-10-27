#include "gui.hpp"
#include "root.hpp"
namespace ui {
void GUI::set_connected(bool set) {
  screen.Post([this, set]() {
    root->connected = set;
    screen.RequestAnimationFrame();
  });
}
void GUI::set_unlocked(bool set) {
  screen.Post([this, set]() {
    root->unlocked = set;
    screen.RequestAnimationFrame();
  });
}
void GUI::run() { screen.Loop(root); }

GUI::GUI(CreateToken)
    : screen(ScreenInteractive::TerminalOutput()), spinnerFactory(screen),
      root(Make<RootComponent>(*this)) {}
} // namespace ui
