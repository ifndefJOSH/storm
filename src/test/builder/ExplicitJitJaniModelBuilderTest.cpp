#include "gtest/gtest.h"
#include "storm-config.h"
#include "storm/models/sparse/StandardRewardModel.h"
#include "storm/models/sparse/MarkovAutomaton.h"
#include "storm/settings/SettingMemento.h"
#include "storm/parser/PrismParser.h"
#include "storm/builder/jit/ExplicitJitJaniModelBuilder.h"
#include "storm/storage/jani/Model.h"

#include "storm/settings/SettingsManager.h"
#include "storm/settings/modules/IOSettings.h"

TEST(ExplicitJitJaniModelBuilderTest, Dtmc) {
    storm::prism::Program program = storm::parser::PrismParser::parse(STORM_CPP_TESTS_BASE_PATH "/builder/die.pm");
    storm::jani::Model janiModel = program.toJani();
    
    std::shared_ptr<storm::models::sparse::Model<double>> model = storm::builder::jit::ExplicitJitJaniModelBuilder<double>(janiModel).build();
    EXPECT_EQ(13ul, model->getNumberOfStates());
    EXPECT_EQ(20ul, model->getNumberOfTransitions());
    
    program = storm::parser::PrismParser::parse(STORM_CPP_TESTS_BASE_PATH "/builder/brp-16-2.pm");
    janiModel = program.toJani();
    model = storm::builder::jit::ExplicitJitJaniModelBuilder<double>(janiModel).build();
    EXPECT_EQ(677ul, model->getNumberOfStates());
    EXPECT_EQ(867ul, model->getNumberOfTransitions());
    
    program = storm::parser::PrismParser::parse(STORM_CPP_TESTS_BASE_PATH "/builder/crowds-5-5.pm");
    janiModel = program.toJani();
    model = storm::builder::jit::ExplicitJitJaniModelBuilder<double>(janiModel).build();
    EXPECT_EQ(8607ul, model->getNumberOfStates());
    EXPECT_EQ(15113ul, model->getNumberOfTransitions());
    
    program = storm::parser::PrismParser::parse(STORM_CPP_TESTS_BASE_PATH "/builder/leader-3-5.pm");
    janiModel = program.toJani();
    model = storm::builder::jit::ExplicitJitJaniModelBuilder<double>(janiModel).build();
    EXPECT_EQ(273ul, model->getNumberOfStates());
    EXPECT_EQ(397ul, model->getNumberOfTransitions());
    
    program = storm::parser::PrismParser::parse(STORM_CPP_TESTS_BASE_PATH "/builder/nand-5-2.pm");
    janiModel = program.toJani();
    model = storm::builder::jit::ExplicitJitJaniModelBuilder<double>(janiModel).build();
    EXPECT_EQ(1728ul, model->getNumberOfStates());
    EXPECT_EQ(2505ul, model->getNumberOfTransitions());
}

TEST(ExplicitJitJaniModelBuilderTest, Ctmc) {
    // Set the PRISM compatibility mode temporarily. It is set to its old value once the returned object is destructed.
    std::unique_ptr<storm::settings::SettingMemento> enablePrismCompatibility = storm::settings::mutableIOSettings().overridePrismCompatibilityMode(true);
    
    storm::prism::Program program = storm::parser::PrismParser::parse(STORM_CPP_TESTS_BASE_PATH "/builder/cluster2.sm");
    storm::jani::Model janiModel = program.toJani();
    
    std::shared_ptr<storm::models::sparse::Model<double>> model = storm::builder::jit::ExplicitJitJaniModelBuilder<double>(janiModel).build();
    EXPECT_EQ(276ul, model->getNumberOfStates());
    EXPECT_EQ(1120ul, model->getNumberOfTransitions());
    
    program = storm::parser::PrismParser::parse(STORM_CPP_TESTS_BASE_PATH "/builder/embedded2.sm");
    janiModel = program.toJani();
    model = storm::builder::jit::ExplicitJitJaniModelBuilder<double>(janiModel).build();
    EXPECT_EQ(3478ul, model->getNumberOfStates());
    EXPECT_EQ(14639ul, model->getNumberOfTransitions());
    
    program = storm::parser::PrismParser::parse(STORM_CPP_TESTS_BASE_PATH "/builder/polling2.sm");
    janiModel = program.toJani();
    model = storm::builder::jit::ExplicitJitJaniModelBuilder<double>(janiModel).build();
    EXPECT_EQ(12ul, model->getNumberOfStates());
    EXPECT_EQ(22ul, model->getNumberOfTransitions());
    
    program = storm::parser::PrismParser::parse(STORM_CPP_TESTS_BASE_PATH "/builder/fms2.sm");
    janiModel = program.toJani();
    model = storm::builder::jit::ExplicitJitJaniModelBuilder<double>(janiModel).build();
    EXPECT_EQ(810ul, model->getNumberOfStates());
    EXPECT_EQ(3699ul, model->getNumberOfTransitions());
    
    program = storm::parser::PrismParser::parse(STORM_CPP_TESTS_BASE_PATH "/builder/tandem5.sm");
    janiModel = program.toJani();
    model = storm::builder::jit::ExplicitJitJaniModelBuilder<double>(janiModel).build();
    EXPECT_EQ(66ul, model->getNumberOfStates());
    EXPECT_EQ(189ul, model->getNumberOfTransitions());
}

TEST(ExplicitJitJaniModelBuilderTest, Mdp) {
    storm::prism::Program program = storm::parser::PrismParser::parse(STORM_CPP_TESTS_BASE_PATH "/builder/two_dice.nm");
    storm::jani::Model janiModel = program.toJani();
    
    std::shared_ptr<storm::models::sparse::Model<double>> model = storm::builder::jit::ExplicitJitJaniModelBuilder<double>(janiModel).build();
    EXPECT_EQ(169ul, model->getNumberOfStates());
    EXPECT_EQ(436ul, model->getNumberOfTransitions());
    
    program = storm::parser::PrismParser::parse(STORM_CPP_TESTS_BASE_PATH "/builder/leader3.nm");
    janiModel = program.toJani();
    model = storm::builder::jit::ExplicitJitJaniModelBuilder<double>(janiModel).build();
    EXPECT_EQ(364ul, model->getNumberOfStates());
    EXPECT_EQ(654ul, model->getNumberOfTransitions());
    
    program = storm::parser::PrismParser::parse(STORM_CPP_TESTS_BASE_PATH "/builder/coin2-2.nm");
    janiModel = program.toJani();
    model = storm::builder::jit::ExplicitJitJaniModelBuilder<double>(janiModel).build();
    EXPECT_EQ(272ul, model->getNumberOfStates());
    EXPECT_EQ(492ul, model->getNumberOfTransitions());
    
    program = storm::parser::PrismParser::parse(STORM_CPP_TESTS_BASE_PATH "/builder/csma2-2.nm");
    janiModel = program.toJani();
    model = storm::builder::jit::ExplicitJitJaniModelBuilder<double>(janiModel).build();
    EXPECT_EQ(1038ul, model->getNumberOfStates());
    EXPECT_EQ(1282ul, model->getNumberOfTransitions());
    
    program = storm::parser::PrismParser::parse(STORM_CPP_TESTS_BASE_PATH "/builder/firewire3-0.5.nm");
    janiModel = program.toJani();
    model = storm::builder::jit::ExplicitJitJaniModelBuilder<double>(janiModel).build();
    EXPECT_EQ(4093ul, model->getNumberOfStates());
    EXPECT_EQ(5585ul, model->getNumberOfTransitions());
    
    program = storm::parser::PrismParser::parse(STORM_CPP_TESTS_BASE_PATH "/builder/wlan0-2-2.nm");
    janiModel = program.toJani();
    model = storm::builder::jit::ExplicitJitJaniModelBuilder<double>(janiModel).build();
    EXPECT_EQ(37ul, model->getNumberOfStates());
    EXPECT_EQ(59ul, model->getNumberOfTransitions());
}

TEST(ExplicitJitJaniModelBuilderTest, Ma) {
    storm::prism::Program program = storm::parser::PrismParser::parse(STORM_CPP_TESTS_BASE_PATH "/builder/simple.ma");
    storm::jani::Model janiModel = program.toJani();
    
    std::shared_ptr<storm::models::sparse::Model<double>> model = storm::builder::jit::ExplicitJitJaniModelBuilder<double>(janiModel).build();
    EXPECT_EQ(5ul, model->getNumberOfStates());
    EXPECT_EQ(8ul, model->getNumberOfTransitions());
    ASSERT_TRUE(model->isOfType(storm::models::ModelType::MarkovAutomaton));
    EXPECT_EQ(4ul, model->as<storm::models::sparse::MarkovAutomaton<double>>()->getMarkovianStates().getNumberOfSetBits());
    
    program = storm::parser::PrismParser::parse(STORM_CPP_TESTS_BASE_PATH "/builder/hybrid_states.ma");
    janiModel = program.toJani();
    model = storm::builder::jit::ExplicitJitJaniModelBuilder<double>(janiModel).build();
    EXPECT_EQ(5ul, model->getNumberOfStates());
    EXPECT_EQ(13ul, model->getNumberOfTransitions());
    ASSERT_TRUE(model->isOfType(storm::models::ModelType::MarkovAutomaton));
    EXPECT_EQ(5ul, model->as<storm::models::sparse::MarkovAutomaton<double>>()->getMarkovianStates().getNumberOfSetBits());
    
    program = storm::parser::PrismParser::parse(STORM_CPP_TESTS_BASE_PATH "/builder/stream2.ma");
    janiModel = program.toJani();
    model = storm::builder::jit::ExplicitJitJaniModelBuilder<double>(janiModel).build();
    EXPECT_EQ(12ul, model->getNumberOfStates());
    EXPECT_EQ(14ul, model->getNumberOfTransitions());
    ASSERT_TRUE(model->isOfType(storm::models::ModelType::MarkovAutomaton));
    EXPECT_EQ(7ul, model->as<storm::models::sparse::MarkovAutomaton<double>>()->getMarkovianStates().getNumberOfSetBits());
}

TEST(ExplicitJitJaniModelBuilderTest, FailComposition) {
    storm::prism::Program program = storm::parser::PrismParser::parse(STORM_CPP_TESTS_BASE_PATH "/builder/system_composition.nm");
    storm::jani::Model janiModel = program.toJani();
    
    ASSERT_THROW(storm::builder::jit::ExplicitJitJaniModelBuilder<double>(janiModel).build(), storm::exceptions::WrongFormatException);
}

TEST(ExplicitJitJaniModelBuilderTest, IllegalSynchronizingWrites) {
    storm::prism::Program program = storm::parser::PrismParser::parse(STORM_CPP_TESTS_BASE_PATH "/builder/coin2-2-illegalSynchronizingWrite.nm");
    storm::jani::Model janiModel = program.toJani();
    storm::builder::BuilderOptions options;
    options.setExplorationChecks(true);
    
    ASSERT_THROW(storm::builder::jit::ExplicitJitJaniModelBuilder<double>(janiModel, options).build(), storm::exceptions::WrongFormatException);
}
