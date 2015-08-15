# Author: Brian M. Clapper, G. Varoquaux, Lars Buitinck
# License: BSD

from numpy.testing import assert_array_equal

import numpy as np

from scipy.optimize import linear_sum_assignment


def test_linear_sum_assignment():
    for cost_matrix, expected_cost in [
        # Square
        ([[400, 150, 400],
          [400, 450, 600],
          [300, 225, 300]],
         [150, 400, 300]
         ),

        # Rectangular variant
        ([[400, 150, 400, 1],
          [400, 450, 600, 2],
          [300, 225, 300, 3]],
         [150, 2, 300]),

        # Square
        ([[10, 10, 8],
          [9, 8, 1],
          [9, 7, 4]],
         [10, 1, 7]),

        # Rectangular variant
        ([[10, 10, 8, 11],
          [9, 8, 1, 1],
          [9, 7, 4, 10]],
         [10, 1, 4]),

        # n == 2, m == 0 matrix
        ([[], []],
         []),
    ]:
        cost_matrix = np.array(cost_matrix)
        row_ind, col_ind = linear_sum_assignment(cost_matrix)
        assert_array_equal(row_ind, np.sort(row_ind))
        assert_array_equal(expected_cost, cost_matrix[row_ind, col_ind])

        row_ind, col_ind = linear_sum_assignment(cost_matrix.T)
        assert_array_equal(row_ind, np.sort(row_ind))
        assert_array_equal(expected_cost, cost_matrix[row_ind, col_ind])
