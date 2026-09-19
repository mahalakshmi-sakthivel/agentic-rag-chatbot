#include <gtest/gtest.h>
#include "orchestrator/agent/intent_detector.h"
#include "orchestrator/agent/planner.h"
#include "orchestrator/agent/executor.h"
#include "orchestrator/agent/evaluator.h"
#include "orchestrator/context/context_builder.h"
#include "orchestrator/tools/tool_registry.h"
#include "orchestrator/tools/vector_search_tool.h"
#include "orchestrator/tools/structured_query_tool.h"
#include "orchestrator/tools/calculator_tool.h"
#include "orchestrator/schemas/agent_state.h"
#include "orchestrator/schemas/orchestrator_context.h"
#include "common/uuid.h"

using namespace orchestrator;
using namespace common;

TEST(IntentDetectorTest, AnalyzesIntentsCorrectly) {
    IntentDetector detector;
    EXPECT_EQ(detector.analyze("calculate the revenue"), IntentType::CALCULATION);
    EXPECT_EQ(detector.analyze("what is the highest revenue?"), IntentType::STRUCTURED_DATA_QUERY);
    EXPECT_EQ(detector.analyze("compare Q1 and Q2"), IntentType::COMPARISON);
    EXPECT_EQ(detector.analyze("what was the revenue?"), IntentType::CLARIFICATION_REQUIRED);
    EXPECT_EQ(detector.analyze("hello there"), IntentType::NO_RETRIEVAL_REQUIRED);
    EXPECT_EQ(detector.analyze("tell me about the project"), IntentType::DOCUMENT_LOOKUP);
}

TEST(PlannerTest, CreatesCorrectPlan) {
    Planner planner;
    AgentState state;
    
    state.intent = IntentType::DOCUMENT_LOOKUP;
    auto plan1 = planner.create_plan(state);
    ASSERT_EQ(plan1.size(), 1);
    EXPECT_EQ(plan1[0].action, "vector_search");

    state.intent = IntentType::COMPARISON;
    auto plan2 = planner.create_plan(state);
    ASSERT_EQ(plan2.size(), 2);
    EXPECT_EQ(plan2[0].action, "vector_search");
    EXPECT_EQ(plan2[1].action, "vector_search");
}

TEST(ExecutorTest, ToolExecutionAndLimits) {
    auto registry = std::make_shared<ToolRegistry>();
    registry->register_tool(std::make_shared<VectorSearchTool>());
    registry->register_tool(std::make_shared<StructuredQueryTool>());
    registry->register_tool(std::make_shared<CalculatorTool>());

    AgentLimits limits;
    limits.max_total_tool_calls = 2; // Strict limit for testing
    Executor executor(registry, limits);

    AgentState state;
    state.identity.tenant_id = "tenant-test";
    state.normalized_query = "What was Q3 revenue?";
    state.plan = {
        {1, "vector_search", "Search"},
        {2, "structured_query", "Query"},
        {3, "calculator", "Calc"} // Will hit limit before executing
    };

    bool step1 = executor.execute_step(state);
    EXPECT_TRUE(step1);
    EXPECT_EQ(state.tool_results.size(), 1);
    EXPECT_EQ(state.tool_results[0].status, "success");

    bool step2 = executor.execute_step(state);
    EXPECT_TRUE(step2);
    EXPECT_EQ(state.tool_results.size(), 2);

    // Step 3 should fail due to limit
    bool step3 = executor.execute_step(state);
    EXPECT_FALSE(step3);
    EXPECT_EQ(state.tool_results.size(), 3);
    EXPECT_EQ(state.tool_results[2].status, "failed");
    EXPECT_EQ(state.tool_results[2].error.value()["code"], "AGENT_EXECUTION_LIMIT");
}

TEST(ContextBuilderTest, PreservesSources) {
    AgentState state;
    state.query_id = "q-123";
    state.identity.tenant_id = "t-1";
    state.original_query = "test";
    state.normalized_query = "test";
    
    nlohmann::json chunk = {
        {"chunk_id", "c-123"},
        {"document_id", "d-1"},
        {"text", "test content"},
        {"score", 0.9},
        {"source_location", "page 1"},
        {"filename", "test.pdf"}
    };
    state.retrieved_chunks.push_back(chunk);
    
    ToolResult mock_tr;
    mock_tr.tool_name = "vector_search";
    mock_tr.status = "success";
    state.tool_results.push_back(mock_tr);

    ContextBuilder builder;
    OrchestratorContext ctx = builder.build(state);

    EXPECT_EQ(ctx.query_id, "q-123");
    EXPECT_EQ(ctx.identity.tenant_id, "t-1");
    ASSERT_EQ(ctx.retrieved_chunks.size(), 1);
    EXPECT_EQ(ctx.retrieved_chunks[0]["chunk_id"], "c-123");
    EXPECT_EQ(ctx.retrieved_chunks[0]["source_location"], "page 1");
    
    nlohmann::json j = ctx.to_json();
    EXPECT_TRUE(j.contains("query_id"));
    EXPECT_TRUE(j.contains("identity"));
    EXPECT_TRUE(j.contains("retrieved_chunks"));
    EXPECT_TRUE(j.contains("steps_taken"));
}

TEST(QueryRefinerTest, RefinesQueryCorrectly) {
    AgentState state;
    state.retrieval_attempts = 1;
    state.normalized_query = "revenue";
    state.intent = IntentType::DOCUMENT_LOOKUP;
    state.plan = {{1, "vector_search", "Search"}};
    
    // Need at least one tool result to allow refinement
    ToolResult tr;
    tr.tool_name = "vector_search";
    tr.status = "success";
    state.tool_results.push_back(tr);

    QueryRefiner refiner;
    bool refined = refiner.refine(state);
    
    EXPECT_TRUE(refined);
    EXPECT_EQ(state.normalized_query, "Q3 total revenue 2025");
    EXPECT_EQ(state.plan.size(), 2);
    EXPECT_EQ(state.plan[1].action, "vector_search");
}

TEST(AgentTest, EndToEndExecution) {
    auto registry = std::make_shared<ToolRegistry>();
    registry->register_tool(std::make_shared<VectorSearchTool>());
    registry->register_tool(std::make_shared<StructuredQueryTool>());
    registry->register_tool(std::make_shared<CalculatorTool>());

    Agent agent(registry);
    common::IdentityContext id;
    id.tenant_id = "t-1";
    
    auto ctx = agent.process_query("What was the highest monthly revenue?", id, "q-1");
    EXPECT_EQ(ctx.query_id, "q-1");
    // Should route to structured_query
    ASSERT_EQ(ctx.steps_taken.size(), 1);
    EXPECT_EQ(ctx.steps_taken[0].tool, "structured_query");
    
    auto ctx2 = agent.process_query("What was the revenue?", id, "q-2");
    EXPECT_TRUE(ctx2.needs_clarification);
    
    auto ctx3 = agent.process_query("Calculate 15% of 800.", id, "q-3");
    ASSERT_EQ(ctx3.steps_taken.size(), 1);
    EXPECT_EQ(ctx3.steps_taken[0].tool, "calculator");
}
