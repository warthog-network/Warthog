## Token Creation

In colloquial language in DeFi, the term *token* can have two different meanings: It can refer to a token class (like "WART") but also to a single unit (like "1 WART") of a specific token class. To avoid ambiguity we will distinguish explicitly between token class and token unit.

### Creation Rate Limit
Each day a limited number of new token classes can be created. For now this number will be set to 1 to avoid token flood on Warthog and increase visibility of new tokens.

### Initial Distribution
There are three basic distribution strategies for the balance of newly created token classes. These strategies can also be combined.
1. *Own-All*: The address creating the tokens owns all token units after creation
2. *Clone-Balance*: Token unit distribution is cloned from some other existing token class
3. *Proportional-To-Investment*: Token units are distributed proportionally to the amount of another token class spent an initial auction.

Whenever a balance distribution is cloned, we need to take a snapshot of the balance of all holders. With a copy-on-write strategy, this can be cheaply implemented but still, cloning balance distribution should not occur too often, like no more than once a week and at specific block heights. 

### Token Creation Modes
Warthog will support a number of different token creation modes that determine firstly how tokens are initially created, and secondly, where raised money, if any, goes to.
The following creation modes are supported:

#### Classic
Creator of the token class owns full balance, i.e. initial distribution is Own-All.

**Use case**: Dynamic supply like USDT.

#### Dividend
If we have a token class *`A`* we can pay a dividend in *`B`* tokens to all holders of `A`. We need two ingredients for that:
1. Creating a new token *`ADIV`* for representing `A` dividends with balance distribution cloned from `A`.
2. Peg `ADIV` to `B` at specific rate by locking appropriate amount of `B` for swap from `ADIV` to `B`.

In this setting the dividend needs to be explicitly "claimed" by swapping the dividend `ADIV` tokens to `B` tokens.

**Use case**: Paying dividends for assets, airdrops

#### Auction
In this creation mode an auction is held during which addresses can spend a balance of a specified token class (e.g. `WART`)  which proportionally convert to units of the newly minted tokens at the end of the auction. Like in crowdfunding campaigns, a minimal target can be specified which must be achieved for the conversion to take place, otherwise the transactions will be reverted. If succeeded, the raised funds will be transferred to the account that created the token.

**Use case**: ICOs, crowdfunding, fair initial token distribution

#### Summary:
Creation Mode| Initial Distribution | Raised funds go to
-------------|-----------|---------
*Classic* | Own-All  | Token creator
*Dividend* | Clone-Balance  | no funds raised
*Auction* | Proportional-To-Investment | Token creator






## Token Hash

<!-- static constexpr size_t bytesize = 16 + 3 + 2 + 32 + 20 + 8 + 65; -->

byte range | content
-----------|---------
1-16  | transaction id
17-19 | 3 reserved bytes
20-21 | compact fee



