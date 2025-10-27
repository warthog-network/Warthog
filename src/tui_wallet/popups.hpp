#pragma once
#include "gui.hpp"
#include "validated_input.hpp"
// #include "include/ftxui/component/component.hpp"

namespace ui {
inline auto validator{[](const std::string &s) { return s.length() > 2; }};
using namespace ftxui;
struct PopupBase : public ComponentBase {
protected:
  bool closed{false};

public:
  bool is_closed() const { return closed; }
};

struct TransferPopup : public GUIComponent,
                       public PopupBase,
                       std::enable_shared_from_this<TransferPopup> {
private:
  AssetNameHash asset;
  bool isLiquidity{false}; // whether this transfer is for liquidity
  std::shared_ptr<ui::LabeledValidatedBase> amount;
  std::shared_ptr<ui::LabeledValidatedBase> toAddr;
  std::shared_ptr<ui::LabeledValidatedBase> fee;
  std::shared_ptr<ui::LabeledValidatedBase> nonceId;
  Component btnCancel;
  Component btnCreate;

public:
  Element OnRender() override {
    using namespace std::string_literals;
    auto content = [&]() {
      if (isLiquidity) {
                amount->label = "Amount ("s+asset.liquidity_name()+"-): ";
        return vbox({text("Pool: "s + asset.market()),
                     text("Base Asset: " + asset.to_string()), toAddr, amount,
                     fee, nonceId});
      } else {
                amount->label = "Amount ("s+asset.name+"): ";
        return vbox({text("Asset: " + asset.to_string()), toAddr, amount, fee,
                     nonceId});
      }
    }();
    auto title{"New "s + (isLiquidity ? "Liquidity " : "Asset ") + "Transfer"s};
    return vbox({window(text(title), content),
                 hbox(btnCancel, btnCreate->Render()) | center});
  }
  void on_create();
  void on_cancel();
  TransferPopup(GUI &gui, AssetNameHash asset, bool isLiquidity);
};

struct SwapPopup : public GUIComponent,
                   public PopupBase,
                   std::enable_shared_from_this<SwapPopup> {
private:
  AssetNameHash asset;
  std::vector<std::string> swap_directions;
  int side_selected = 0;

  std::shared_ptr<ui::LabeledValidatedBase> amount;
  std::shared_ptr<ui::LabeledValidatedBase> limit;
  std::shared_ptr<ui::LabeledValidatedBase> fee;
  Component toggle;
  Component btnCancel;
  Component btnCreate;

public:
  Element OnRender() override {
    amount->label = std::string("Amount (") +
                    (side_selected == 0 ? "WART" : asset.name) + "): ";
    return vbox(
        {window(text("New Swap"),
                vbox({text("Base Asset: " + asset.to_string()),
                      hbox(text("Swap direction: "), toggle->Render()),
                      amount->Render(), limit->Render(), fee->Render()})),
         hbox(btnCancel, btnCreate->Render()) | center});
  }
  void on_create();
  void on_cancel();
  SwapPopup(GUI &gui, AssetNameHash);
};
struct FarmPopup : public GUIComponent, public PopupBase,
                   std::enable_shared_from_this<FarmPopup> {
private:
  AssetNameHash asset;
  std::vector<std::string> liquidity_actions;
  int side_selected = 0;

  std::shared_ptr<ui::LabeledValidatedBase> wart;
  std::shared_ptr<ui::LabeledValidatedBase> base;
  std::shared_ptr<ui::LabeledValidatedBase> limit;
  std::shared_ptr<ui::LabeledValidatedBase> fee;
  Component toggle;
  Component btnCancel;
  Component btnCreate;

public:
  Element OnRender() override {
    base->label = "Max. Amount (" + asset.name+ "): ";
    return vbox(
        {window(text("Farm"),
                vbox({text("Base Asset: " + asset.to_string()),
                      hbox(text("Liquidity action: "), toggle->Render()),
                      wart->Render(), limit->Render(), fee->Render()})),
         hbox(btnCancel, btnCreate->Render()) | center});
  }
  void on_create();
  void on_cancel();
  FarmPopup(GUI &gui, AssetNameHash);
};

} // namespace ui
