#pragma once

class Panel {
public:
	virtual ~Panel() = default;

	virtual bool initialize() { return true; }
	virtual void cleanup() {}

	virtual void update() {}

	virtual void render() {};
	virtual void renderMenu() {};
};