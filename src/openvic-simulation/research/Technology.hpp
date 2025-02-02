#pragma once

#include <cstdint>

#include "openvic-simulation/economy/BuildingType.hpp"
#include "openvic-simulation/military/Unit.hpp"
#include "openvic-simulation/misc/Modifier.hpp"
#include "openvic-simulation/scripts/ConditionalWeight.hpp"
#include "openvic-simulation/types/Date.hpp"
#include "openvic-simulation/types/OrderedContainers.hpp"

namespace OpenVic {
	struct TechnologyFolder : HasIdentifier {
		friend struct TechnologyManager;

	private:
		TechnologyFolder(std::string_view new_identifier);

	public:
		TechnologyFolder(TechnologyFolder&&) = default;
	};

	struct TechnologyArea : HasIdentifier {
		friend struct TechnologyManager;

	private:
		TechnologyFolder const& PROPERTY(folder);

		TechnologyArea(std::string_view new_identifier, TechnologyFolder const& new_folder);

	public:
		TechnologyArea(TechnologyArea&&) = default;
	};

	struct Technology : Modifier {
		friend struct TechnologyManager;

		using unit_set_t = ordered_set<Unit const*>;
		using building_set_t = ordered_set<BuildingType const*>;

	private:
		TechnologyArea const& PROPERTY(area);
		const Date::year_t PROPERTY(year);
		const fixed_point_t PROPERTY(cost);
		const bool PROPERTY(unciv_military);
		const uint8_t PROPERTY(unit);
		unit_set_t PROPERTY(activated_buildings);
		building_set_t PROPERTY(activated_units);
		ConditionalWeight PROPERTY(ai_chance);

		Technology(
			std::string_view new_identifier, TechnologyArea const& new_area, Date::year_t new_year, fixed_point_t new_cost,
			bool new_unciv_military, uint8_t new_unit, unit_set_t&& new_activated_units,
			building_set_t&& new_activated_buildings, ModifierValue&& new_values, ConditionalWeight&& new_ai_chance
		);

		bool parse_scripts(GameManager const& game_manager);

	public:
		Technology(Technology&&) = default;
	};

	struct TechnologySchool : Modifier {
		friend struct TechnologyManager;

	private:
		TechnologySchool(std::string_view new_identifier, ModifierValue&& new_values);
	};

	struct TechnologyManager {
		IdentifierRegistry<TechnologyFolder> IDENTIFIER_REGISTRY(technology_folder);
		IdentifierRegistry<TechnologyArea> IDENTIFIER_REGISTRY(technology_area);
		IdentifierRegistry<Technology> IDENTIFIER_REGISTRY_CUSTOM_PLURAL(technology, technologies);
		IdentifierRegistry<TechnologySchool> IDENTIFIER_REGISTRY(technology_school);

	public:
		bool add_technology_folder(std::string_view identifier);

		bool add_technology_area(std::string_view identifier, TechnologyFolder const* folder);

		bool add_technology(
			std::string_view identifier, TechnologyArea const* area, Date::year_t year, fixed_point_t cost,
			bool unciv_military, uint8_t unit, Technology::unit_set_t&& activated_units,
			Technology::building_set_t&& activated_buildings, ModifierValue&& values, ConditionalWeight&& ai_chance
		);

		bool add_technology_school(std::string_view identifier, ModifierValue&& values);

		bool load_technology_file_areas(ast::NodeCPtr root); // common/technology.txt
		bool load_technology_file_schools(ModifierManager const& modifier_manager, ast::NodeCPtr root); // also common/technology.txt
		bool load_technologies_file(
			ModifierManager const& modifier_manager, UnitManager const& unit_manager,
			BuildingTypeManager const& building_type_manager, ast::NodeCPtr root
		); // technologies/*.txt
		bool generate_modifiers(ModifierManager& modifier_manager) const;

		bool parse_scripts(GameManager const& game_manager);
	};
}