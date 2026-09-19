# Phase 5: Agentic Orchestrator

This directory (`backend/src/orchestrator`) contains the implementation of Phase 5.

## Components
* `agent/` - Contains the `Agent` which runs the execution loop, `IntentDetector`, `Planner`, `Executor`, and `Evaluator`.
* `context/` - `ContextBuilder` which formats the `OrchestratorContext` for Phase 6.
* `schemas/` - Data definitions for AgentState, ToolResult, and OrchestratorContext.
* `tools/` - `ToolRegistry` and implementations (including mocks for Phase 4 `vector_search` and `structured_query`, and a stub `calculator`).

## Integration with Phase 4
Phase 5 orchestrates queries by using the `ToolRegistry`. The Phase 4 RAG engine will eventually provide real implementations of `VectorSearchTool` and `StructuredQueryTool`. Currently, they are mocked in `backend/src/orchestrator/tools/` and return contract-compliant data with standard `chunk_id`, `document_id`, `score`, and `source_location` fields. Once Phase 4 is ready, the mocks can simply be swapped for the real tools in `agent.cpp` or main server initialization.

## Integration with Phase 6
Phase 5's ultimate output is the `OrchestratorContext` struct which strictly adheres to Section 14.2 of the Technical Contract. This context is serialized to JSON and should be passed to Phase 6's LLM pipeline. Phase 5 does **not** call the LLM itself, thereby preserving separation of concerns.

## How to Build and Run Tests
Ensure you have CMake installed and the dependencies outlined in Phase 1 (GTest, nlohmann_json).

1. Change directory to the `backend/` root.
2. Configure with CMake: `cmake --preset apt` or `cmake -B build`
3. Build the backend and tests: `cmake --build build`
4. Run the orchestrator tests: `./build/agentic_rag_backend_tests` (this will automatically discover and run the `orchestrator` tests added to the test suite).
