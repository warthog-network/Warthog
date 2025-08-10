#pragma once
#include "include/ftxui/component/screen_interactive.hpp"
namespace ui {
using namespace ftxui;
struct SpinnerWorker {
private:
  ScreenInteractive &screen;
  std::jthread t;

public:
  int spinnerStep;
  SpinnerWorker(ScreenInteractive &screen);
};

struct SpinnerHandle {
private:
  std::shared_ptr<SpinnerWorker> handle;

public:
  SpinnerHandle(std::shared_ptr<SpinnerWorker> h) : handle(std::move(h)) {}
  int get_step() { return handle->spinnerStep; }
};

struct SpinnerFactory {
  using ptr_t = std::shared_ptr<SpinnerWorker>;

private:
  ScreenInteractive &screen;
  mutable ptr_t handle;

public:
  SpinnerFactory(ScreenInteractive &screen) : screen(screen) {}
  SpinnerHandle get_handle() const;
};

struct SpinnerRenderer {
  SpinnerHandle handle; // to keep the animation running
  SpinnerRenderer(SpinnerHandle h) : handle(h) {}
  Element render(int spinnerType) {
    return spinner(spinnerType, handle.get_step());
  }
};
} // namespace ui
