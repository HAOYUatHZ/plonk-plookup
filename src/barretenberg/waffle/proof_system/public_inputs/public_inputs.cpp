#include "./public_inputs.hpp"

using namespace barretenberg;

namespace waffle {

/**
 * Public inputs!
 *
 * This is a linear-time method of evaluating public inputs, that doesn't require modifications
 * to any pre-processed selector polynomials.
 *
 * We validate public inputs by using a transition constraint to force every public input's
 * copy permutation to be unbalanced. We then directly compute the 'delta' factor required
 * to re-balance the permutation, and add it back into the grand product.
 *
 * Ok, let's wind back to the start. Let's say we have 'n' public inputs.
 *
 * We reserve the first 'n' rows of program memory for public input validation.
 * For each of these constraints, we *force* the first column's cell to be zero, using
 * a standard arithmetic gate (i.e. w_i = 0 for the first i rows)
 *
 * We then apply a copy constraint between the first two columns in program memory.
 * i.e. for each row, the second cell is a copy of the first.
 *
 * We then apply a copy constraint that maps the second cell to memory cell in the circuit,
 * to whererever the public input in question is required.
 *
 * This creates an unbalanced permutation. For the arithmetic constraint to be valid, the first
 * cell must be 0.
 *
 * But for the copy permutation to be valid, the first cell must be our public input!
 *
 * We make a further modification to the copy permutation argument. For the forced-zero cells,
 * the *correct* permutation term for sigma_1(g_i) would be k.g_i , where k is a coset generator
 * that maps to the second column.
 *
 * However the actual permutation term for sigma_1(g_i) is just g_i
 *
 * This makes the permutation product, for the targeted zero-value public input cells, equal to 1
 * 
 * We use the following notation:
 * 
 *   (*) n is the size of a multiplicative subgroup H
 * 
 *   (*) g  are the elements of multiplicative subgroup H
 *        i
 *   (*) w     is the i'th witness in column j
 *        i, j
 *   (*) β, γ are random challenges generated by the verifier
 * 
 *   (*) σ     are the values of the j'th copy permutation selector polynomial
 *        i, j
 * 
 *   (*) k  are coset generators, such that g  . k  is not an element of H, or the coset
 *        j                                  i    j
 *       produced by any other k  value, for all l != j
 *                              l
 * 
 * THIS is our normal permutation grand product:
 * 
 *        n
 *      ━┳━━┳━ /                       \   /                        \   /                        \  
 *       ┃  ┃  | w     +  β . g    + γ |   | w     + β . k . g  + γ |   | w     + β . k . g  + γ |
 *       ┃  ┃  |  i, 1         i       |   |  i, 2        1   i     |   |  i, 3        2   i     | 
 *       ┃  ┃  | ━━━━━━━━━━━━━━━━━━━━━ | . | ━━━━━━━━━━━━━━━━━━━━━━ | . | ━━━━━━━━━━━━━━━━━━━━━━ | = Z
 *       ┃  ┃  | w     + β . σ     + γ |   | w     + β . σ     + γ  |   | w     + β . σ     + γ  | 
 *      i = 1  \  i, 1        i, 1     /   \  i, 2        i, 2      /   \  i, 3        i, 3      /
 * 
 * 
 * Now let's say that we have m public inputs. We transform the first m products involving column 1, into the following:
 * 
 * 
 *   m                                        m
 * ━┳━━┳━ /                       \         ━┳━━┳━ /               \ 
 *  ┃  ┃  | w     +  β . g    + γ |          ┃  ┃  | 0 + β . g + γ | 
 *  ┃  ┃  |  i, 1         i       |  =====>  ┃  ┃  |          i    | = 1
 *  ┃  ┃  | ━━━━━━━━━━━━━━━━━━━━━ |          ┃  ┃  | ━━━━━━━━━━━━━ | 
 *  ┃  ┃  | w     + β . σ     + γ |          ┃  ┃  | 0 + β . g + γ | 
 * i = 1  \  i, 1        i, 1     /         i = 1  \          i    / 
 * 
 * 
 * We now define a 'delta' term that can be publicly computed, which is the inverse of the following product:
 * 
 * 
 *   m                              
 * ━┳━━┳━ /                        \ 
 *  ┃  ┃  | w     + β . g      + γ | 
 *  ┃  ┃  |  i, 1        i         |    1
 *  ┃  ┃  | ━━━━━━━━━━━━━━━━━━━━━━ | =  ━
 *  ┃  ┃  | w     + β . k . g  + γ |    δ
 * i = 1  \  i, 1            i     / 
 *
 * 
 * i.e. we apply an explicit copy constraint that maps w     to w      for the first m witnesses
 *                                                      i, 1     i, 2
 * 
 * After applying these transformations, we have
 * 
 *    Z  =  δ
 *     n
 *          
 * 
 * This can be validated by verifying that
 * 
 *  (Z(X.g) - δ).L   (X) = 0 mod Z'(X)
 *                n-1             H
 * 
 * We check the n-1'th evaluation of Z(X.g), as opposed to the n'th evaluation of Z(X), because
 * we need to cut the n'th subgroup element out of our vanishing polynomial Z'(X), as
 *                                                                           H
 * the grand product polynomial identity does not hold at this subgroup element.
 * 
 * This validates the correctness of the public inputs. Specifically, that for the first m rows of program memory,
 * the memory cells on the second column map to our public inputs. We can then use traditional copy constraints to map
 * these cells to other locations in program memory.
 **/
fr::field_t compute_public_input_delta(const std::vector<barretenberg::fr::field_t>& inputs,
                                       const fr::field_t& beta,
                                       const fr::field_t& gamma,
                                       const fr::field_t& subgroup_generator)
{
    fr::field_t numerator = fr::field_t::one;
    fr::field_t denominator = fr::field_t::one;

    fr::field_t work_root = fr::field_t::one;
    fr::field_t T0;
    fr::field_t T1;
    fr::field_t T2;
    for (const auto& witness : inputs) {
        T0 = witness + gamma;
        T1 = work_root * beta;
        T2 = T1 * fr::field_t::coset_generators[0];
        T1 += T0;
        T2 += T0;
        numerator *= T2;
        denominator *= T1;
        work_root *= subgroup_generator;
    }
    denominator = denominator.invert();
    T0 = denominator * numerator;
    return T0;
}
} // namespace waffle