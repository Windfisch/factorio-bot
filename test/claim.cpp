#include "../inventory.hpp"
#include <iostream>
#include <memory>
using namespace std;

TaggedAmount am;
shared_ptr<Task> tasks[10];

static void claim(int i, int n)
{
	cout << "claiming " << n << " for task " << i << " gave " << am.claim(tasks[i], n);
	cout << "\tfree -> " << (am.amount - am.n_claimed());
	for (auto& c : am.claims)
		cout << ", " << c.owner.lock()->priority << " -> " << c.amount;
	cout << endl;
}

int main()
{
	for (int i=0; i<10; i++)
	{
		tasks[i] = make_shared<Task>();
		tasks[i]->priority = i;
	}

	am.amount = 100;

	claim(5,20);
	claim(5,30);
	claim(2,40);
	claim(7,30);
	claim(3,5);
	claim(1,99);
	claim(0,300);
	claim(4,30);

	return 0;
}
