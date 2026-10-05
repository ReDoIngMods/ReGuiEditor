#include "Resource.hpp"

#include <rend/ResourceManager.hpp>

rend::Resource::Resource() {
	rend::ResourceManager::getInstance().registerResource(this);
}

rend::Resource::~Resource() {
	rend::ResourceManager::getInstance().unregisterResource(this);
}