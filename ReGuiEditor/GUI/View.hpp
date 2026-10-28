#pragma once

class View {
public:
	virtual ~View() = default;

	virtual bool initialize() { return true; }
	virtual void deinitialize() {}

	virtual void render() {}
	virtual void renderMenu() {}

	virtual void update(double deltaTime) {}
	virtual void tick() {}
};