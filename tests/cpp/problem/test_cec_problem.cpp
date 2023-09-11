#include "../utils.hpp"
#include "ioh/problem/cec.hpp"
#include "ioh/problem/cec/cec_problem.hpp"

TEST_F(BaseTest, CECProblem)
{
    const char* ioh_resources_cstr = std::getenv("IOH_RESOURCES");
    if (ioh_resources_cstr == nullptr)
    {
        LOG("Could not load cec_problem.in. Point the environment variable IOH_RESOURCES to the static/ folder of IOHexperimenter.");
        std::exit(EXIT_FAILURE);
    }
    std::string ioh_resources_path(ioh_resources_cstr);
    const auto file_name = ioh_resources_path + "/cec_problem.in";
    std::ifstream infile(file_name.c_str());
    ASSERT_TRUE(infile.is_open());

    const auto &problem_factory = ioh::problem::ProblemRegistry<ioh::problem::CEC>::instance();
    std::string s;
    while (getline(infile, s))
    {
        auto tmp = split(s, " ");
        if (tmp.empty()) { continue; }

        auto func_id = stoi(tmp[0]);
        auto ins_id = stoi(tmp[1]);
        auto x = string_to_vector_double(tmp[2]);
        auto f = stod(tmp[3]);

        auto instance = problem_factory.create(func_id, ins_id, x.size());
        auto y = (*instance)(x);
        EXPECT_LE(abs(y - f) / f, 1.0 / pow(10, 6 - log(10)))
            << "The fitness of function " << func_id << "( ins "
            << ins_id << " ) is " << f << " ( not " << y << ").";
    }
}

TEST_F(BaseTest, xopt_equals_yopt_cec)
{
    const auto& problem_factory = ioh::problem::ProblemRegistry<ioh::problem::CEC>::instance();
    for (const auto& name : problem_factory.names())
    {
        // Any function's, but the composition function's, optimum is defined.
        if (!(name == "CEC_CompositionFunction1" || name == "CEC_CompositionFunction2" || name == "CEC_CompositionFunction3" || name == "CEC_CompositionFunction4"))
        {
            auto instance = problem_factory.create(name, 1, 10);
            auto&& x = instance->optimum().x;
            EXPECT_DOUBLE_EQ(instance->optimum().y, (*instance)(x)) << *instance;
        }
    }
}
