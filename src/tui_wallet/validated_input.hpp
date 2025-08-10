#pragma once
#include "include/ftxui/component/event.hpp"
#include "include/ftxui/component/component.hpp"
namespace ui {
using namespace ftxui;
using StringValidator = std::function<bool(std::string)>;


struct ValidatedBase : public ComponentBase {
  bool valid{false};
  std::string content;
  ValidatedBase(const ValidatedBase &) = delete;
  bool OnEvent(Event event) override {
    if (event == Event::ArrowUp || event == Event::ArrowDown)
      return false;
    return ComponentBase::OnEvent(std::move(event));
  }
  ValidatedBase(StringValidator validator) {
    InputOption option;
    option.multiline = false;
    option.content = &content;
    option.on_change = [this, validator = std::move(validator)]() {
      valid = validator(content);
    };
    option.transform = [&](InputState state) {
      auto element{state.element};
      element |= color(valid ? Color::Green : Color::Red);
      if (state.focused) {
        return element | inverted;
      }
      return element;
    };
    Add(Input(option));
  }
};
inline Component Validated(StringValidator validator) {
  return Make<ValidatedBase>(std::move(validator));
}

struct LabeledValidatedBase : public ValidatedBase {
  using ValidatedBase::ValidatedBase;
  std::string label;
  LabeledValidatedBase(std::string label, StringValidator validator)
      : ValidatedBase(std::move(validator)), label(std::move(label)) {}
  Element OnRender() override {

    return hbox({
        text(label) | color(valid ? Color::Green : Color::Red),
        ValidatedBase::OnRender(),
    });
  };
};
inline std::shared_ptr<LabeledValidatedBase>
LabeledValidated(std::string label, StringValidator validator) {
  return Make<LabeledValidatedBase>(std::move(label), std::move(validator));
}
} // namespace ui
