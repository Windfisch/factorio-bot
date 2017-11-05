#include <string>
#include <vector>

struct Recipe
{
	struct ItemAmount
	{
		const ItemPrototype* item;
		double amount; // this is mostly int, but some recipes have probabilities.
		               // in this case, the average value is stored here.

		ItemAmount(const ItemPrototype* item_, double amount_) : item(item_),amount(amount_) {}
	};

	std::string name;
	bool enabled;
	double energy;

	std::vector<ItemAmount> ingredients;
	std::vector<ItemAmount> products;
};
