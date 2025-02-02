#include "Deployment.hpp"

#include "openvic-simulation/GameManager.hpp" /* gosh don't we all just love circular inclusion :DDD */

using namespace OpenVic;
using namespace OpenVic::NodeTools;

Leader::Leader(
	std::string_view new_name, Unit::branch_t new_branch, Date new_date, LeaderTrait const* new_personality,
	LeaderTrait const* new_background, fixed_point_t new_prestige, std::string_view new_picture
) : name { new_name }, branch { new_branch }, date { new_date }, personality { new_personality }, background { new_background },
	prestige { new_prestige }, picture { new_picture } {}

Regiment::Regiment(std::string_view new_name, LandUnit const* new_type, Province const* new_home)
	: name { new_name }, type { new_type }, home { new_home } {}

Ship::Ship(std::string_view new_name, NavalUnit const* new_type) : name { new_name }, type { new_type } {}

Army::Army(std::string_view new_name, Province const* new_location, std::vector<Regiment>&& new_regiments)
	: name { new_name }, location { new_location }, regiments { std::move(new_regiments) } {}

Navy::Navy(std::string_view new_name, Province const* new_location, std::vector<Ship>&& new_ships)
	: name { new_name }, location { new_location }, ships { std::move(new_ships) } {}

Deployment::Deployment(
	std::string_view new_path, std::vector<Army>&& new_armies, std::vector<Navy>&& new_navies,
	std::vector<Leader>&& new_leaders
) : HasIdentifier { new_path }, armies { std::move(new_armies) }, navies { std::move(new_navies) },
	leaders { std::move(new_leaders) } {}

bool DeploymentManager::add_deployment(
	std::string_view path, std::vector<Army>&& armies, std::vector<Navy>&& navies, std::vector<Leader>&& leaders
) {
	if (path.empty()) {
		Logger::error("Attemped to load order of battle with no path! Something is very wrong!");
		return false;
	}

	return deployments.add_item(
		std::make_unique<Deployment>(std::move(path), std::move(armies), std::move(navies), std::move(leaders))
	);
}

bool DeploymentManager::load_oob_file(
	GameManager const& game_manager, Dataloader const& dataloader, std::string_view history_path,
	Deployment const*& deployment, bool fail_on_missing
) {
	deployment = get_deployment_by_identifier(history_path);
	if (deployment != nullptr) {
		return true;
	}
	if (missing_oob_files.contains(history_path)) {
		return !fail_on_missing;
	}
	static constexpr std::string_view oob_directory = "history/units/";
	const fs::path lookedup_path =
		dataloader.lookup_file(StringUtils::append_string_views(oob_directory, history_path), false);
	if (lookedup_path.empty()) {
		missing_oob_files.emplace(history_path);
		if (fail_on_missing) {
			Logger::warning("Could not find OOB file ", history_path, "!");
			return false;
		} else {
			return true;
		}
	}
	std::vector<Army> armies;
	std::vector<Navy> navies;
	std::vector<Leader> leaders;
	bool ret = expect_dictionary_keys_and_default(
		key_value_success_callback, // TODO: load SOI information
		"leader", ZERO_OR_MORE, [&leaders, &game_manager](ast::NodeCPtr node) -> bool {
			std::string_view leader_name {};
			Unit::branch_t leader_branch = Unit::branch_t::INVALID_BRANCH;
			Date leader_date {};
			LeaderTrait const* leader_personality = nullptr;
			LeaderTrait const* leader_background = nullptr;
			fixed_point_t leader_prestige = 0;
			std::string_view picture {};

			bool ret = expect_dictionary_keys(
				"name", ONE_EXACTLY, expect_identifier_or_string(assign_variable_callback(leader_name)),
				"date", ONE_EXACTLY, expect_identifier_or_string(expect_date_str(assign_variable_callback(leader_date))),
				"type", ONE_EXACTLY, UnitManager::expect_branch_identifier(assign_variable_callback(leader_branch)),
				"personality", ONE_EXACTLY,
					game_manager.get_military_manager().get_leader_trait_manager().expect_leader_trait_identifier_or_string(
						assign_variable_callback_pointer(leader_personality)
					),
				"background", ONE_EXACTLY,
					game_manager.get_military_manager().get_leader_trait_manager().expect_leader_trait_identifier_or_string(
						assign_variable_callback_pointer(leader_background)
					),
				"prestige", ZERO_OR_ONE, expect_fixed_point(assign_variable_callback(leader_prestige)),
				"picture", ZERO_OR_ONE, expect_identifier_or_string(assign_variable_callback(picture))
			)(node);

			if (leader_personality != nullptr && !leader_personality->is_personality_trait()) {
				Logger::error(
					"Leader ", leader_name, " has personality ", leader_personality->get_identifier(),
					" which is not a personality trait!"
				);
				ret = false;
			}
			if (leader_background != nullptr && !leader_background->is_background_trait()) {
				Logger::error(
					"Leader ", leader_name, " has background ", leader_background->get_identifier(),
					" which is not a background trait!"
				);
				ret = false;
			}
			leaders.emplace_back(
				leader_name, leader_branch, leader_date, leader_personality, leader_background, leader_prestige, picture
			);
			return ret;
		},
		"army", ZERO_OR_MORE, [&armies, &game_manager](ast::NodeCPtr node) -> bool {
			std::string_view army_name {};
			Province const* army_location = nullptr;
			std::vector<Regiment> army_regiments {};

			const bool ret = expect_dictionary_keys(
				"name", ONE_EXACTLY, expect_string(assign_variable_callback(army_name)),
				"location", ONE_EXACTLY,
					game_manager.get_map().expect_province_identifier(assign_variable_callback_pointer(army_location)),
				"regiment", ONE_OR_MORE, [&game_manager, &army_regiments](ast::NodeCPtr node) -> bool {
					std::string_view regiment_name {};
					LandUnit const* regiment_type = nullptr;
					Province const* regiment_home = nullptr;
					const bool ret = expect_dictionary_keys(
						"name", ONE_EXACTLY, expect_string(assign_variable_callback(regiment_name)),
						"type", ONE_EXACTLY, game_manager.get_military_manager().get_unit_manager()
							.expect_land_unit_identifier(assign_variable_callback_pointer(regiment_type)),
						"home", ZERO_OR_ONE, game_manager.get_map()
							.expect_province_identifier(assign_variable_callback_pointer(regiment_home))
					)(node);
					if (regiment_home == nullptr) {
						Logger::warning("Regiment ", regiment_name, " has no home province!");
					}
					army_regiments.emplace_back(regiment_name, regiment_type, regiment_home);
					return ret;
				},
				/* Another paradox gem, tested in game and they don't lead the army or even show up */
				"leader", ZERO_OR_MORE, success_callback
			)(node);
			armies.emplace_back(army_name, army_location, std::move(army_regiments));
			return ret;
		},
		"navy", ZERO_OR_MORE, [&navies, &game_manager](ast::NodeCPtr node) -> bool {
			std::string_view navy_name {};
			Province const* navy_location = nullptr;
			std::vector<Ship> navy_ships {};

			const bool ret = expect_dictionary_keys(
				"name", ONE_EXACTLY, expect_string(assign_variable_callback(navy_name)),
				"location", ONE_EXACTLY,
					game_manager.get_map().expect_province_identifier(assign_variable_callback_pointer(navy_location)),
				"ship", ONE_OR_MORE, [&game_manager, &navy_ships](ast::NodeCPtr node) -> bool {
					std::string_view ship_name {};
					NavalUnit const* ship_type = nullptr;
					const bool ret = expect_dictionary_keys(
						"name", ONE_EXACTLY, expect_string(assign_variable_callback(ship_name)),
						"type", ONE_EXACTLY, game_manager.get_military_manager().get_unit_manager()
							.expect_naval_unit_identifier(assign_variable_callback_pointer(ship_type))
					)(node);
					navy_ships.emplace_back(ship_name, ship_type);
					return ret;
				},
				/* Another paradox gem, tested in game and they don't lead the army or even show up */
				"leader", ZERO_OR_MORE, success_callback
			)(node);
			navies.emplace_back(navy_name, navy_location, std::move(navy_ships));
			return ret;
		}
	)(Dataloader::parse_defines(lookedup_path).get_file_node());
	ret &= add_deployment(history_path, std::move(armies), std::move(navies), std::move(leaders));
	deployment = get_deployment_by_identifier(history_path);
	if (deployment == nullptr) {
		ret = false;
	}
	return ret;
}

size_t DeploymentManager::get_missing_oob_file_count() const {
	return missing_oob_files.size();
}
