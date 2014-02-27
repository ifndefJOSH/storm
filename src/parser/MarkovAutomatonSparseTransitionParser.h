#ifndef STORM_PARSER_MARKOVAUTOMATONSPARSETRANSITIONPARSER_H_
#define STORM_PARSER_MARKOVAUTOMATONSPARSETRANSITIONPARSER_H_

#include "src/storage/SparseMatrix.h"
#include "src/storage/BitVector.h"
#include "Parser.h"

namespace storm {
    namespace parser {
        
        /*
         * A class providing the functionality to parse the transitions of a Markov automaton.
         */
        class MarkovAutomatonSparseTransitionParser {
        public:
            /*
             * A structure representing the result of the first pass of this parser. It contains the number of non-zero entries in the model, the highest state index
             * and the total number of choices.
             */
            struct FirstPassResult {
                
                FirstPassResult() : numberOfNonzeroEntries(0), highestStateIndex(0), numberOfChoices(0) {
                    // Intentionally left empty.
                }

                // The total number of non-zero entries of the model.
                uint_fast64_t numberOfNonzeroEntries;
                
                // The highest state index that appears in the model.
                uint_fast64_t highestStateIndex;
                
                // The total number of choices in the model.
                uint_fast64_t numberOfChoices;
            };
            
            /*
             * A structure representing the result of the parser. It contains the sparse matrix that represents the transitions (along with a vector indicating
             * at which index the choices of a given state begin) as well as the exit rates for all Markovian choices.
             */
            struct ResultType {
                
                ResultType(FirstPassResult const& firstPassResult) : transitionMatrix(), nondeterministicChoiceIndices(firstPassResult.highestStateIndex + 2), markovianChoices(firstPassResult.numberOfChoices), markovianStates(firstPassResult.highestStateIndex + 1), exitRates(firstPassResult.highestStateIndex + 1) {
                    // Intentionally left empty.
                }
                
                // A matrix representing the transitions of the model.
                storm::storage::SparseMatrix<double> transitionMatrix;
                
                // A vector indicating which rows of the matrix represent the choices of a given state.
                std::vector<uint_fast64_t> nondeterministicChoiceIndices;
                
                // A bit vector indicating which choices are Markovian. By duality, all other choices are probabilitic.
                storm::storage::BitVector markovianChoices;
                
                // A bit vector indicating which states possess a Markovian choice.
                storm::storage::BitVector markovianStates;
                
                // A vector that stores the exit rates for each
                std::vector<double> exitRates;
            };
            
            /*!
             * Parses the given file under the assumption that it contains a Markov automaton specified in the appropriate format.
             *
             * @param filename The name of the file to parse.
             * @return A structure representing the result of the parser.
             */
            static ResultType parseMarkovAutomatonTransitions(std::string const& filename);
            
        private:
            /*
             * Performs the first pass on the input pointed to by the given buffer.
             *
             * @param buffer The buffer that cointains the input.
             * @param lineEndings The line endings that are to be used while parsing.
             * @return A structure representing the result of the first pass.
             */
            static FirstPassResult firstPass(char* buffer, SupportedLineEndingsEnum lineEndings);
            
            /*
             * Performs the second pass on the input pointed to by the given buffer with the information of the first pass.
             *
             * @param buffer The buffer that cointains the input.
             * @param lineEndings The line endings that are to be used while parsing.
             * @param firstPassResult The result of the first pass performed on the same input.
             * @return A structure representing the result of the second pass.
             */
            static ResultType secondPass(char* buffer, SupportedLineEndingsEnum lineEndings, FirstPassResult const& firstPassResult);
        };
        
    } // namespace parser
} // namespace storm

#endif /* STORM_PARSER_MARKOVAUTOMATONSPARSETRANSITIONPARSER_H_ */