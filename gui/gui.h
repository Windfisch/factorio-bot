#include <memory>

class FactorioGame;

namespace GUI
{

class _MapGui_impl;

class MapGui
{
	public:
		MapGui(const FactorioGame* game);
		~MapGui();

	private:
		std::unique_ptr<_MapGui_impl> impl;
};

double wait(double t);

} //namespace GUI
