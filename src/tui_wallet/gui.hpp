#pragma once
#include "include/ftxui/component/screen_interactive.hpp"
#include "gui_fwd.hpp"
#include "spinner.hpp"
#include "ui_data.hpp"

namespace ui {
using namespace ftxui;
struct RootComponent;
struct GUIComponent {
protected:
  [[nodiscard]] static ftxui::ScreenInteractive &extract_screen(GUI &);
  [[nodiscard]] static RootComponent &extract_root(GUI &);
  GUIComponent(GUI &gui) : gui(gui) {}
  GUI &gui;

public:
  ScreenInteractive &gui_screen() const;
  RootComponent &gui_root() const;
};
struct NamedComponentBase : public ComponentBase {
  NamedComponentBase(std::string windowName, Components children = {})
      : ComponentBase(std::move(children)), windowName(windowName) {}
  std::string windowName;
};

using NamedComponent = std::shared_ptr<NamedComponentBase>;
struct NamedGUIComponent : public ui::GUIComponent, public NamedComponentBase {
  NamedGUIComponent(GUI &gui, std::string windowName, Components children = {})
      : GUIComponent(gui),
        NamedComponentBase(std::move(windowName), std::move(children)) {}
};
template <typename T> struct MakeTab : public NamedGUIComponent {
private:
  T &get() { return *static_cast<T *>(this); }

public:
  using NamedGUIComponent::NamedGUIComponent;
};

struct GUI : public SelectedAsset, public std::enable_shared_from_this<GUI> {
  friend RootComponent;
  friend GUIComponent;

public:
  ScreenInteractive screen;

private:
  ui::SpinnerFactory spinnerFactory;
  std::shared_ptr<RootComponent> root;

  struct CreateToken {};

public:
  SpinnerRenderer get_spinner() const { return spinnerFactory.get_handle(); }
  void set_connected(bool set);
  void set_unlocked(bool set);
  static std::shared_ptr<GUI> create_instance() {
    return std::make_shared<GUI>(CreateToken());
  }
  GUI(CreateToken);

  void terminate() { screen.Exit(); }
  void run();
};

} // namespace ui
