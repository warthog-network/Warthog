#include "popups.hpp"
#include "root.hpp"
#include "tabs.hpp"
#include "transaction.hpp"
namespace ui {

void TransferPopup::on_cancel() { closed = true; }

void TransferPopup::on_create() {
  auto properties{TransactionProperties{
      .title{"Transfer"},
      .entries{
          {"Asset ", asset.to_string()},
          {"Amount (" + asset.name + ") ", amount.get()->content},
          {"Destination ", toAddr.get()->content},
          {"Fee (WART) ", fee.get()->content},
          {"NonceId ", nonceId.get()->content},
      }}};
  if (isLiquidity) {
    properties.entries.push_back(
        {"NOTE: ",
         "This transfer is for pool liquidity, not the actual asset!"});
  }

  onconfirm_generator_t generator{
      [tr = shared_from_this()](result_cb_t cb) -> std::function<void()> {
        return [tr = std::move(tr), cb = std::move(cb)]() mutable {
          std::thread t([tr = std::move(tr), cb = std::move(cb)] {
            tr->closed = true;
            std::this_thread::sleep_for(std::chrono::seconds(2));
            cb("Success", "Transaction was sent");
          });
          t.detach();
        };
      }};
  gui_root().popup_confirmation(std::move(properties), std::move(generator));
};

TransferPopup::TransferPopup(GUI &gui, AssetNameHash a, bool isLiquidity)
    : GUIComponent(gui), asset(std::move(a)), isLiquidity(isLiquidity),
      amount(ui::LabeledValidated("Amount:  ", validator)),
      toAddr(ui::LabeledValidated("Destination: ", validator)),
      fee(ui::LabeledValidated("Fee (WART): ", validator)),
      nonceId(ui::LabeledValidated("NonceId: ", validator)),
      btnCancel(Button("Cancel", [&]() { this->on_cancel(); })),
      btnCreate(Button("Create", [&]() { this->on_create(); })) {
  Add(Container::Vertical({toAddr, amount, fee, nonceId,
                           Container::Horizontal({btnCancel, btnCreate})}));
}

void SwapPopup::on_create() {
  auto properties{TransactionProperties{
      .title{"Swap"},
      .entries{
          {"From Token ",
           "95ae6efb2f4fe5e4fd3a5b21df7f755f878383610505fe64 (WART)"},
          {"To Token ",
           "95ae6efb2f4fe5e4fd3a5b21df7f755f878383610505fe64 (WART)"},
          {"Amount", amount.get()->content},
          {"Limit Price ", limit.get()->content},
      }}};

  onconfirm_generator_t generator{
      [tr = shared_from_this()](result_cb_t cb) -> std::function<void()> {
        return [tr = std::move(tr), cb = std::move(cb)]() mutable {
          std::thread t([tr = std::move(tr), cb = std::move(cb)] {
            tr->closed = true;
            std::this_thread::sleep_for(std::chrono::seconds(2));
            cb("Success", "Transaction was sent");
          });
          t.detach();
        };
      }};
  gui_root().popup_confirmation(std::move(properties), std::move(generator));
};
void SwapPopup::on_cancel() { closed = true; }

SwapPopup::SwapPopup(GUI &gui, AssetNameHash a)
    : GUIComponent(gui), asset(std::move(a)),
      swap_directions{"BUY " + asset.name + " WITH WART",
                      "SELL " + asset.name + " FOR WART"},
      amount(ui::LabeledValidated("Amount:  ", validator)),
      limit(ui::LabeledValidated("Limit Price:  ", validator)),
      fee(ui::LabeledValidated("Fee (WART):  ", validator)),
      toggle(Toggle(swap_directions, &side_selected)),
      btnCancel(Button("Cancel", [&]() { this->on_cancel(); })),
      btnCreate(Button("Create", [&]() { this->on_create(); })) {
  Add(Container::Vertical({toggle, amount, limit, fee,
                           Container::Horizontal({btnCancel, btnCreate})}));
}
void FarmPopup::on_create() {
  auto properties{TransactionProperties{
      .title{"Farm"},
      .entries{
          {"From Token ",
           "95ae6efb2f4fe5e4fd3a5b21df7f755f878383610505fe64 (WART)"},
          {"To Token ",
           "95ae6efb2f4fe5e4fd3a5b21df7f755f878383610505fe64 (WART)"},
          {"Amount", wart.get()->content},
          {"Limit Price ", limit.get()->content},
      }}};

  onconfirm_generator_t generator{
      [tr = shared_from_this()](result_cb_t cb) -> std::function<void()> {
        return [tr = std::move(tr), cb = std::move(cb)]() mutable {
          std::thread t([tr = std::move(tr), cb = std::move(cb)] {
            tr->closed = true;
            std::this_thread::sleep_for(std::chrono::seconds(2));
            cb("Success", "Transaction was sent");
          });
          t.detach();
        };
      }};
  gui_root().popup_confirmation(std::move(properties), std::move(generator));
};
void FarmPopup::on_cancel() { closed = true; }

FarmPopup::FarmPopup(GUI &gui, AssetNameHash a)
    : GUIComponent(gui), asset(std::move(a)),
      liquidity_actions{"DEPOSIT LIQUIDITY",
                      "WITHDRAW LIQUIDITY"},
      wart(ui::LabeledValidated("Max. Amount (WART):  ", validator)),
      base(ui::LabeledValidated("", validator)),
      limit(ui::LabeledValidated("Limit Price:  ", validator)),
      fee(ui::LabeledValidated("Fee (WART):  ", validator)),
      toggle(Toggle(liquidity_actions, &side_selected)),
      btnCancel(Button("Cancel", [&]() { this->on_cancel(); })),
      btnCreate(Button("Create", [&]() { this->on_create(); })) {

  Add(Container::Vertical({toggle, wart, limit, fee,
                           Container::Horizontal({btnCancel, btnCreate})}));
}
} // namespace ui
