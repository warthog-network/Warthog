#pragma once
#include "include/ftxui/dom/table.hpp"
#include "gui.hpp"
#include "popups.hpp"
#include "spinner.hpp"
#include "transaction.hpp"
#include "validated_input.hpp"
#include <cmath>
#include "include/ftxui/component/component.hpp" // for Dropdown, Renderer, Container
#include "include/ftxui/component/screen_interactive.hpp" // for ScreenInteractive
#include "include/ftxui/dom/elements.hpp"                 // for text, vbox, hbox
#include <string>
#include <vector>

using namespace ftxui;
using result_cb_t = std::function<void(std::string, std::string)>;
using onconfirm_generator_t = std::function<std::function<void()>(result_cb_t)>;
namespace ui {

struct AutocompleteInputImpl : public ComponentBase {
  std::string input_text;
  InputOption input_option;
  Component input;

private:
  std::vector<std::string> filtered;
  int focus_index = -1;

public:
  AutocompleteInputImpl() {
    input_option.on_change = [&] {
      if (!input_text.empty())
        filtered = filter(input_text);
      else
        filtered.clear();
      focus_index = 0;
    };
    input = Input(&input_text, "Type...", input_option);
    Add(input);
  }
  AutocompleteInputImpl(const AutocompleteInputImpl &) = delete;

  // bool Focusable() const override { return true; }
  //
  // void SetFocus(bool focus) override { input->SetFocus(focus); }

  Element OnRender() override {
    auto input_element = input->Render();

    Element suggestions_element;
    if (!filtered.empty()) {
      std::vector<Element> suggestion_elems;
      for (int i = 0; i < (int)filtered.size(); ++i) {
        auto elem = text(filtered[i]);
        if (i == focus_index)
          elem |= inverted;
        suggestion_elems.push_back(elem);
      }
      suggestions_element = vbox(std::move(suggestion_elems)) | borderLight |
                            size(HEIGHT, LESS_THAN, 7);
    } else {
      suggestions_element = text("");
    }

    return vbox({
        input_element,
        suggestions_element,
    });
  };
  bool OnEvent(Event event) override {
    if (!filtered.empty()) {
      if (event == Event::ArrowDown) {
        focus_index += 1;
        if (focus_index == (int)filtered.size())
          focus_index = 0;
        return true;
      }
      if (event == Event::ArrowUp) {
        focus_index =
            (focus_index - 1 + (int)filtered.size()) % (int)filtered.size();
        return true;
      }
      if ((event == Event::Return || event == Event::Tab) &&
          focus_index != -1) {
        // Keyboard confirmation: accept suggestion
        input_text = filtered[focus_index];
        input_option.cursor_position = 0;
        filtered.clear();
        focus_index = -1;
        return true;
      }
    }
    return input->OnEvent(std::move(event));
  };

  std::vector<std::string> suggestions = {
      "apple", "ape",    "aave",      "applause",   "apply",     "banana",
      "grape", "orange", "pineapple", "strawberry", "watermelon"};

  std::vector<std::string> filter(std::string_view sv) const {
    std::vector<std::string> res;
    for (auto &s : suggestions) {
      if (s.starts_with(sv))
        res.push_back(s);
    }
    return res;
  };
};

inline Component AutocompleteInput() { return Make<AutocompleteInputImpl>(); }

struct AboutTab : public MakeTab<AboutTab> {
  Component address;
  Component amount;
  Component nonceId;

  Element OnRender() {
    return vbox({address->Render(), amount->Render(), nonceId->Render()});
  }
  AboutTab(GUI &gui)
      : MakeTab(gui, "About"),
        address(ui::LabeledValidated("About:  ", validator)),
        amount(ui::LabeledValidated("Amount:  ", validator)),
        nonceId(ui::LabeledValidated("NonceId: ", validator)) {
    Add(Container::Vertical({address, amount, nonceId}));
  }
};

inline ButtonOption ButtonRoundOption() {
  ButtonOption option;
  option.transform = [](const EntryState &s) {
    auto element = text(s.label) | border;
    if (s.focused) {
      element |= inverted;
    }
    return element;
  };
  return option;
}

struct TransactionDetailsBase : public ComponentBase {
private:
  TransactionProperties properties;

public:
  TransactionDetailsBase(TransactionProperties p) : properties(std::move(p)) {}
  Element OnRender() override {
    std::vector<std::vector<Element>> initArg;
    for (auto &[name, val] : properties.entries)
      initArg.push_back({text(name), text(val)});
    ftxui::Table table(std::move(initArg));
    return window(text(properties.title) | center, table.Render());
  }
};

inline Component TransactionDetails(TransactionProperties properties) {
  return Make<TransactionDetailsBase>(std::move(properties));
  // return Renderer([p = std::move(properties)]() -> Element {
  //   std::vector<std::vector<std::string>> initArg;
  //   for (auto &[name, value] : p.entries) {
  //     initArg.push_back({name, value});
  //   }
  //   ftxui::Table table(std::move(initArg));
  //   return window(text(p.title) | center, table.Render());
  // });
}

struct NotificationPopupBase : public ui::PopupBase {
private:
  std::string title, message;
  Element linesElement;
  Component btnOk;

public:
  NotificationPopupBase(std::string title, std::string message)
      : title(std::move(title)), message(std::move(message)),
        btnOk(Button("OK", [this]() { closed = true; })) {
    Add(btnOk);
  }
  Element OnRender() override {
    return window(text(title),
                  vbox(paragraph(message), btnOk->Render() | center));
  }
};
struct ConfirmationComponentBase : public GUIComponent, public ui::PopupBase {
  Component txdetails;
  Component btnCancel;
  Component btnConfirm;
  std::shared_ptr<NotificationPopupBase> resultPopup;
  SpinnerRenderer spinner;
  bool submitting{false};
  std::function<void()> onConfirm;

  ConfirmationComponentBase(GUI &gui, TransactionProperties txprops,
                            onconfirm_generator_t);
  [[nodiscard]] auto result_cb();
  bool OnEvent(Event e) override {
    if (resultPopup)
      return resultPopup->OnEvent(e);
    return PopupBase::OnEvent(e);
  }
  Element OnRender() override {
    return vbox(text("Creating Transaction") | center, txdetails->Render(),
                (submitting
                     ? hbox(text("Submitting transaction"), spinner.render(1))
                     : hbox(btnCancel->Render(), btnConfirm->Render())) |
                    center);
  }
};

struct AssetTab : public MakeTab<AssetTab> {
  Component btnTransferAsset;
  Component btnSwap;
  Component btnTransferLiquidity;
  Component btnFarm;

private:
  SpinnerRenderer spinner;

  void on_asset_transfer();
  void on_asset_swap();
  void on_liquidity_transfer();
  void on_liquidity_farm();
  Element OnRender() override {
    return window(text("Actions"),
                  hbox(vbox(text("Asset") | center, separator(),
                            btnTransferAsset->Render(),

                            btnSwap->Render()),
                       separator(),
                       vbox(text("Liquidity") | center, separator(),
                            btnTransferLiquidity, btnFarm->Render())));
  }

public:
  AssetTab(GUI &gui);
};

struct WalletTab : public MakeTab<WalletTab> {
  Component address;
  Component amount;
  Component nonceId;

  Element OnRender() {
    return vbox({address->Render(), amount->Render(), nonceId->Render()});
  }
  WalletTab(GUI &gui)
      : MakeTab(gui, "Wallet"),
        address(ui::LabeledValidated("Wallet:  ", validator)),
        amount(ui::LabeledValidated("Amount:  ", validator)),
        nonceId(ui::LabeledValidated("NonceId: ", validator)) {
    Add(Container::Vertical({address, amount, nonceId}));
  }
};

struct TabsBase : public ComponentBase {
  std::vector<NamedComponent> components;
  std::vector<std::string> tabNames;
  int tab_index{0};
  TabsBase(std::vector<NamedComponent> componentsInit)
      : components(std::move(componentsInit)) {
    for (auto &c : components)
      tabNames.push_back({c->windowName});

    std::vector<Component> v;
    for (auto &c : components)
      v.push_back(c);
    Add(Container::Vertical(
        {Menu(&tabNames, &tab_index, MenuOption::HorizontalAnimated()),
         Container::Tab(std::move(v), &tab_index)}));
  }
};

Component Tabs(auto &&...args) {
  return Make<TabsBase>(std::vector<NamedComponent>{
      NamedComponent(std::forward<decltype(args)>(args))...});
}

} // namespace ui
