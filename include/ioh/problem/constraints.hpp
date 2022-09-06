#pragma once 

#include "ioh/common.hpp"

namespace ioh::problem
{
    namespace constraint
    {
        enum class Enforced
        {
            NOT,    // don't do anything
            HIDDEN, // calculate but don't penalize

                    // The following only have impact on constraint sets
            SOFT,   // penalize, but aggregate all the constraint penalties into a sum 
            HARD    // penalize, and if violation return only penalty for this constraint in contraintset         
        };   
    }
    
    /**
     * @brief Abstract type for constraints
     * @tparam T the data type, matches problem data type
    */
    template <typename T>
    class Constraint : public common::HasRepr
    {
    protected:
        double violation_;
        bool is_feasible_;
    public:
        constraint::Enforced enforced;
        double weight;
        /**
         * @brief Construct a new contraint object
         * @param enforced policy for enforcing the penalty on the constraint see constraint::Enforced
         * @param weight the penalty weight
        */
        Constraint(const constraint::Enforced enforced = constraint::Enforced::NOT, const double weight = 1.0) :
            violation_(0.), is_feasible_(true), enforced(enforced), weight(weight)
        {
        }
        
        /**
         * @brief apply the constraint, and return a (potentially) penalized value for y
         * @param x the search space value
         * @param y the objective space value
         * @return penalized value for y
        */
        [[nodiscard]] double operator()(const std::vector<T> &x, const double y)
        {
            if (!is_feasible(x, y))
                return penalize(y);
            return y;
        }
        
        /**
         * @brief Checks whether the x and y values are in the feasible domain
         * @param x the search space value
         * @param y the objective space value
         * @return true if both x and y are feasible, false otherwise
        */
        [[nodiscard]] bool is_feasible(const std::vector<T> &x, const double y)
        {
            if (enforced == constraint::Enforced::NOT)
                return true;
            
            is_feasible_ = enforced == constraint::Enforced::HIDDEN or !compute_violation(x, y);
            return is_feasible_;
        }

        /**
         * @brief The value for is_feasibile_ is cached, in order not to call
         * compute_violation again. This value corresponse to the last call of is_feasible
         * @return is_feasible_
        */
        [[nodiscard]] bool cached_is_feasible() const { return is_feasible_; }
        
        /**
         * @brief Compute if x or y has any violations of the constraint. This should set violation_.
         * @param x the search space value
         * @param y the objective space value 
         * @return true when there is violation and false otherwise.
        */
        [[nodiscard]] virtual bool compute_violation(const std::vector<T> &x, const double y) = 0;

        /**
         * @brief Penalize the y value. This method is only called when there is a violation, 
         * and is_feasible returns a false value, see operator().
         * @param y the objective space value
         * @return the penalized value for y.
        */
        [[nodiscard]] virtual double penalize(const double y) const { return y + penalty(); }
        
        //! Accessor for violation_
        [[nodiscard]] virtual double violation() const { return violation_; }
        
        //! Accessor for penalty
        [[nodiscard]] virtual double penalty() const { return weight * violation(); }
        
    };

    /**
     * @brief Convienence typedef: shared_ptr of a Contraint
     * @tparam T numeric type for the contraint
    */
    template <typename T>
    using ConstraintPtr = std::shared_ptr<Constraint<T>>;
    
    /**
     * @brief Convienence typedef: vector of ContraintPtr
     * @tparam T numeric type for the contraint
     */
    template <typename T>
    using Constraints = std::vector<ConstraintPtr<T>>;

    /**
     * @brief Wrapper class for multiple constraints. Every problem has an associated
     * ConstrainSet, which can be empty. Constraints can be added and modified via this interface. 
     * @tparam T the numeric type for the Constraint
    */
    template <typename T>
    struct ConstraintSet : Constraint<T>
    {
        Constraints<T> constraints;
        /**
         * @brief Construct a new ConstrainSet object from a set of constrains (default constructor)
         * @param cs the constraints
        */
        ConstraintSet(const Constraints<T> &cs = {}) :
            Constraint<T>(constraint::Enforced::HARD), // ConstraintSet is always enforced
            constraints(cs)
        {
        }

        /**
         * @brief Construct a new ConstrainSet object from a single ConstraintPtr
         * @param c the constraint
         */
        ConstraintSet(const ConstraintPtr<T> &c) : ConstraintSet(Constraints<T>{c}) {}

        /**
         * @brief Override for Contraint<T>::compute_violation. Calls is_feasible for every constraint in order to 
         * check whether any of the constraints is violated.
         * @param x the search space value
         * @param y the objective space value 
         * @return true when there is violation, false otherwise
        */
        [[nodiscard]] bool compute_violation(const std::vector<T> &x, const double y) override
        {
            bool violated = false;
            for (auto &ci : constraints)
                violated = !ci->is_feasible(x, y) || violated;
            return violated;
        }
        
        /**
         * @brief Override for Contraint::penalty(), which is the sum of the penalties of all the constraints in the set
         * @return the aggregated penalty value
        */
        [[nodiscard]] double penalty() const override
        {
            double p = 0.;
            for (const auto &ci : constraints)
                p += ci->penalty();
            return p;
        }

        /**
         * @brief Override for Contraint::violation(), which is the sum of the violations of all the constraints in the set
         * @return the aggregated violation value
         */
        [[nodiscard]] double violation() const override
        {
            double v = 0.;
            for (const auto &ci : constraints)
                v += ci->violation();
            return v;
        }

        /**
         * @brief Override voor penalize. If any of the constraints has a HARD enforcement policy and 
         * is not feasible, the penalize method for that constraint will be returned by this method (by first occurence).
         * Otherwise, y is penalized by y + penalty(), which is the sum of all penalty values for each constraint.
         * 
         * @param y objective space value
         * @return penalized objective value
        */
        [[nodiscard]] double penalize(const double y) const override
        {
            for (const auto &ci : constraints)
                if (ci->enforced == constraint::Enforced::HARD and !ci->cached_is_feasible())
                    return ci->penalize(y);

            return y + penalty();
        }

        /**
         * @brief Add a constraint to the set. If the constraint is already added, do nothing. 
         * @param c the constraint to be added.
        */
        void add(const ConstraintPtr<T> &c) { 
            for (const auto &ci : constraints)
                if (ci == c)
                    return;
            constraints.push_back(c);
        }

        /**
         * @brief Remove a constraint for the set, if it exists.
         * @param c the constraint to be removed.
        */
        void remove(const ConstraintPtr<T> &c)
        {
            for (size_t i = 0; i < constraints.size(); i++)
                if (constraints[i] == c)
                    constraints.erase(constraints.begin() + i--);
        }
                
        /**
         * @brief alias for constraints.size()
         * @return the number of constraints
        */
        [[nodiscard]] size_t n() const { return constraints.size(); }

        
        /**
         * @brief Alias for constraints.at()
         * @param i index
         * @return a reference to constraints[i]
        */
        [[nodiscard]] ConstraintPtr<T> &operator[](size_t i) { return constraints.at(i); }

        //! String representation
        [[nodiscard]] std::string repr() const override
        {
            return fmt::format("<ConstraintSet: {}>", fmt::join(constraints, ","));
        }
    };

        
    //! Box-BoxConstraint object
    template <typename T>
    struct BoxConstraint : Constraint<T>
    {

        //! lower bound
        std::vector<T> lb;

        //! Upper bound
        std::vector<T> ub;

        /**
         * @brief Construct a new BoxConstraint object
         *
         * @param lower lower bound
         * @param upper upper bound
         * @param enforced enforcement policy
         */
        BoxConstraint(const std::vector<T> &lower, const std::vector<T> &upper,
                      constraint::Enforced enforced = constraint::Enforced::NOT                      
        
        ) :
            Constraint<T>(enforced), lb(lower), ub(upper)
        {
        }

        /**
         * @brief Construct a new BoxConstraint object
         *
         * @param size size of the BoxConstraint
         * @param lower lower bound
         * @param upper upper bound
         * @param enforced enforcement policy
         */
        explicit BoxConstraint(const int size = 1, const T lower = std::numeric_limits<T>::lowest(),
                               const T upper = std::numeric_limits<T>::max(),
                               constraint::Enforced enforced = constraint::Enforced::NOT
                               
            ) :
            BoxConstraint(std::vector<T>(size, lower), std::vector<T>(size, upper), enforced)
        {
        }

        /**
         * @brief Initialization helper, resize the constraint. Fills the entire constraint with the first value 
         * both the lower and upper bound.
         * @param s the new size
        */
        void fit(const int s)
        {
            if ((ub.size() != static_cast<size_t>(s)) || (ub.size() != lb.size()))
                IOH_DBG(warning, "Bound dimension is wrong");
            
            ub = std::vector<T>(s, ub.at(0));
            lb = std::vector<T>(s, lb.at(0));
        }

        //! Check if the BoxConstraints are violated, override Constraint::compute_violation
        [[nodiscard]] bool compute_violation(const std::vector<T> &x, const double) override
        {
            this->violation_ = 0.0;
            for (size_t i = 0; i < x.size(); i++)
            {
                double v = 0.;
                if (x[i] > ub[i])
                    v = x[i] - ub[i];
                else if (x[i] < lb[i]) 
                    v = lb[i] - x[i];

                this->violation_ += v * v;
            }

            return this->violation_ > 0.0;
        }

        //! Return resize version of BoxConstraint
        BoxConstraint<T> resize(const int s) const
        {
            return BoxConstraint<T>(std::vector<T>(s, lb.at(0)), std::vector<T>(s, ub.at(0)));
        }

        //! String representation
        std::string repr() const override { return fmt::format("<BoxConstraint lb: [{}] ub: [{}]>", lb, ub); }
    };


    //! BoxConstraint object alias
    template <typename T>
    using Bounds = BoxConstraint<T>;

    //! function that computes a constraint violation, recieves both the x and y values
    template <typename T>
    using ConstraintFunction = std::function<double(const std::vector<T> &, const double)>;

    /**
     * @brief Wrapper for functions, that compute a constraint violation
     * @tparam T the numeric type for the search space values
    */
    template<typename T>
    class FunctionalConstraint : public Constraint<T>
    {
        ConstraintFunction<T> fn_;
        std::string name_;
    public:
        /**
         * @brief Construct a new functional constraint
         * @param fn the function that computes the violation
         * @param weight the constraint penalty weight, used to penalize the y value as weight * violation
         * @param enforced enforcement policy, see constraint::Enforced
         * @param name a name for this constraint, only used for the string representation.
        */
        FunctionalConstraint(ConstraintFunction<T> fn, const double weight = 1.0,
                             const constraint::Enforced enforced = constraint::Enforced::SOFT,
                             const std::string &name = "") : 
            Constraint<T>(enforced, weight),
            fn_(fn), name_(name)
        {
        }

        /**
         * @brief Override for Constraint::compute_violation, calls fn_ to compute violation_ value 
         * @param x the search space value
         * @param y the objective space value  
         * @return true when the absolute value for violation_ > 0 false otherwise
        */
        [[nodiscard]] bool compute_violation(const std::vector<T> &x, const double y) override {
            this->violation_ = fn_(x, y);
            return abs(this->violation_) > 0.0;
        }

        //! Override string representation
        [[nodiscard]] std::string repr() const override { return fmt::format("<FunctionalConstraint {}>", name_); }
    };

} // namespace ioh::problem