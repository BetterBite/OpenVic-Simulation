#pragma once

#include "Types.hpp"

namespace OpenVic2 {
	class Good : HasIdentifier {
		public:
			std::string category;
			price_t cost;
			std::string colour;
			bool isAvailable;
			bool isTradable;
			bool isMoney;
			bool hasOverseasPenalty;

			Good(Good&&) = default;
			Good(std::string const& identifier,std::string const& category, price_t cost, std::string const& colour, 
				bool isAvailable, bool isTradable, bool isMoney, bool hasOverseasPenalty) : HasIdentifier(identifier),
				category(category), cost(cost), colour(colour), isAvailable(isAvailable), isMoney(isMoney), hasOverseasPenalty(hasOverseasPenalty) {};
	};
}
