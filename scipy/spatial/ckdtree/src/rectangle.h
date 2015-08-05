
#ifndef CKDTREE_CPP_RECTANGLE
#define CKDTREE_CPP_RECTANGLE

#include <new>
#include <typeinfo>
#include <stdexcept>
#include <ios>
#include <cmath>
#include <cstring>


#ifndef NPY_UNLIKELY
#define NPY_UNLIKELY(x) (x)
#endif

#ifndef NPY_LIKELY
#define NPY_LIKELY(x) (x)
#endif

extern npy_float64 infinity;

/* Interval arithmetic
 * ===================
 */
 
struct Rectangle {
    
    npy_intp m;
    npy_float64 *mins;
    npy_float64 *maxes;
    
    std::vector<npy_float64> mins_arr;
    std::vector<npy_float64> maxes_arr;

    Rectangle(const npy_intp _m, 
              const npy_float64 *_mins, 
              const npy_float64 *_maxes) : mins_arr(_m), maxes_arr(_m) {

        /* copy array data */
        m = _m;
        mins = &mins_arr[0];
        maxes = &maxes_arr[0];        
        std::memcpy((void*)mins, (void*)_mins, m*sizeof(npy_float64));
        std::memcpy((void*)maxes, (void*)_maxes, m*sizeof(npy_float64));
    };    
         
    Rectangle(const Rectangle& rect) : mins_arr(rect.m), maxes_arr(rect.m) {
        m = rect.m;
        mins = &mins_arr[0];
        maxes = &maxes_arr[0];        
        std::memcpy((void*)mins, (void*)rect.mins, m*sizeof(npy_float64));
        std::memcpy((void*)maxes, (void*)rect.maxes, m*sizeof(npy_float64));         
    };    
    
    Rectangle() : mins_arr(0), maxes_arr(0) {
        m = 0;
        mins = NULL;
        maxes = NULL;
    };
    
};

struct MinMaxDist {
    /* 1-d pieces
     * These should only be used if p != infinity
     */

    static inline void 
    interval_interval_p(const ckdtree * tree, 
                        const Rectangle& rect1, const Rectangle& rect2,
                        const npy_intp k, const npy_float64 p,
                        npy_float64 *min, npy_float64 *max)
    {
        /* Compute the minimum/maximum distance along dimension k between points in
         * two hyperrectangles.
         */
        *min = std::pow(dmax(0, dmax(rect1.mins[k] - rect2.maxes[k],
                              rect2.mins[k] - rect1.maxes[k])),p);
        *max = std::pow(dmax(rect1.maxes[k] - rect2.mins[k], 
                              rect2.maxes[k] - rect1.mins[k]),p);
    }

    static inline void 
    interval_interval_2(const ckdtree * tree,
                        const Rectangle& rect1, const Rectangle& rect2,
                        const npy_intp k,
                        npy_float64 *min, npy_float64 *max)
    {
        /* Compute the minimum/maximum distance along dimension k between points in
         * two hyperrectangles.
         */
        npy_float64 tmp;
        tmp = dmax(0, dmax(rect1.mins[k] - rect2.maxes[k],
                              rect2.mins[k] - rect1.maxes[k]));
        *min = tmp*tmp;
        tmp = dmax(rect1.maxes[k] - rect2.mins[k], 
                              rect2.maxes[k] - rect1.mins[k]);
        *max = tmp*tmp;
    }

    /* These should be used only for p == infinity */

    static inline void
    rect_rect_p_inf(const ckdtree * tree, 
                    const Rectangle& rect1, const Rectangle& rect2,
                    npy_float64 *min, npy_float64 *max)
    {
        /* Compute the minimum/maximum distance between points in two hyperrectangles. */
        npy_intp i;
        npy_float64 min_dist = 0.;
        for (i=0; i<rect1.m; ++i) {
            min_dist = dmax(min_dist, dmax(rect1.mins[i] - rect2.maxes[i],
                                           rect2.mins[i] - rect1.maxes[i]));
        }                                   
        *min = min_dist;
        npy_float64 max_dist = 0.;
        for (i=0; i<rect1.m; ++i) {
            max_dist = dmax(max_dist, dmax(rect1.maxes[i] - rect2.mins[i],
                                           rect2.maxes[i] - rect1.mins[i]));
        }
        *max = max_dist;
    }

    static inline npy_float64 
    distance_p(const ckdtree * tree, 
               const npy_float64 *x, const npy_float64 *y,
               const npy_float64 p, const npy_intp k,
               const npy_float64 upperbound)
    {    
       /*
        * Compute the distance between x and y
        *
        * Computes the Minkowski p-distance to the power p between two points.
        * If the distance**p is larger than upperbound, then any number larger
        * than upperbound may be returned (the calculation is truncated).
        */
        
        npy_intp i;
        npy_float64 r;
        r = 0;
        if (NPY_LIKELY(p==2.)) {
            /*
            for (i=0; i<k; ++i) {
                z = x[i] - y[i];
                r += z*z;
                if (r>upperbound)
                    return r;
            }*/
            return sqeuclidean_distance_double(x,y,k);
        } 
        else if (p==infinity) {
            for (i=0; i<k; ++i) {
                r = dmax(r,dabs(x[i]-y[i]));
                if (r>upperbound)
                    return r;
            }
        } 
        else if (p==1.) {
            for (i=0; i<k; ++i) {
                r += dabs(x[i]-y[i]);
                if (r>upperbound)
                    return r;
            }
        } 
        else {
            for (i=0; i<k; ++i) {
                r += std::pow(dabs(x[i]-y[i]),p);
                if (r>upperbound)
                     return r;
            }
        }
        return r;
    } 
};
struct MinMaxDistBox {
    static inline void _interval_interval_1d (
        npy_float64 min, npy_float64 max,
        npy_float64 *realmin, npy_float64 *realmax,
        const npy_float64 full, const npy_float64 half
    ) 
    {
        /* Minimum and maximum distance of two intervals in a periodic box
         *
         * min and max is the nonperiodic distance between the near
         * and far edges.
         *
         * full and half are the box size and 0.5 * box size.
         *
         * value is returned in realmin and realmax;
         *
         * This function is copied from kdcount, and the convention
         * of is that
         *
         * min = rect1.min - rect2.max
         * max = rect1.max - rect2.min = - (rect2.min - rect1.max)
         *
         * We will fix the convention later.
         * */
        if(max <= 0 || min >= 0) {
            /* do not pass through 0 */
            min = dabs(min);
            max = dabs(max);
            if(min > max) {
                double t = min;
                min = max;
                max = t;
            }
            if(max < half) {
                /* all below half*/
                *realmin = min;
                *realmax = max;
            } else if(min > half) {
                /* all above half */
                *realmax = full - min;
                *realmin = full - max;
            } else {
                /* min below, max above */
                *realmax = half;
                *realmin = dmin(min, full - max);
            }
        } else {
            /* pass though 0 */
            min = -min;
            if(min > max) max = min;
            if(max > half) max = half;
            *realmax = max;
            *realmin = 0;
        }
    }

    /* Periodic MinMaxDist functions */
    static inline void 
    interval_interval_p(const ckdtree * tree, 
                        const Rectangle& rect1, const Rectangle& rect2,
                        const npy_intp k, const npy_float64 p,
                        npy_float64 *min, npy_float64 *max)
    {
        /* Compute the minimum/maximum distance along dimension k between points in
         * two hyperrectangles.
         */
        _interval_interval_1d(rect1.mins[k] - rect2.maxes[k],
                    rect1.maxes[k] - rect2.mins[k], min, max,
                    tree->raw_boxsize_data[k], tree->raw_boxsize_data[k + rect1.m]);

        *min = std::pow(*min, p);
        *max = std::pow(*max, p);
    }

    static inline void 
    interval_interval_2(const ckdtree * tree,
                        const Rectangle& rect1, const Rectangle& rect2,
                        const npy_intp k,
                        npy_float64 *min, npy_float64 *max)
    {
        /* Compute the minimum/maximum distance along dimension k between points in
         * two hyperrectangles.
         */
        _interval_interval_1d(rect1.mins[k] - rect2.maxes[k],
                    rect1.maxes[k] - rect2.mins[k], min, max,
                    tree->raw_boxsize_data[k], tree->raw_boxsize_data[k + rect1.m]);
        *min *= *min;
        *max *= *max;
    }

    /* These should be used only for p == infinity */

    static inline void
    rect_rect_p_inf(const ckdtree * tree, 
                    const Rectangle& rect1, const Rectangle& rect2,
                    npy_float64 *min, npy_float64 *max)
    {
        /* Compute the minimum/maximum distance between points in two hyperrectangles. */
        npy_intp k;
        npy_float64 min_dist = 0.;
        npy_float64 max_dist = 0.;
        k = 0;
        _interval_interval_1d(rect1.mins[k] - rect2.maxes[k],
                    rect1.maxes[k] - rect2.mins[k], min, max,
                    tree->raw_boxsize_data[k], tree->raw_boxsize_data[k + rect1.m]);

        for (k=1; k<rect1.m; ++k) {
            _interval_interval_1d(rect1.mins[k] - rect2.maxes[k],
                        rect1.maxes[k] - rect2.mins[k], &min_dist, &max_dist,
                        tree->raw_boxsize_data[k], tree->raw_boxsize_data[k + rect1.m]);
            *min = dmin(min_dist, *min);
            *max = dmax(max_dist, *max);
        }                                   
    }
    static inline npy_float64 
    distance_p(const ckdtree * tree, 
               const npy_float64 *x, const npy_float64 *y,
               const npy_float64 p, const npy_intp k,
               const npy_float64 upperbound)
    {    
       /*
        * Compute the distance between x and y
        *
        * Computes the Minkowski p-distance to the power p between two points.
       * If the distance**p is larger than upperbound, then any number larger
        * than upperbound may be returned (the calculation is truncated).
        *
        */
        
        npy_intp i;
        npy_float64 r, r1;
        r = 0;
        for (i=0; i<k; ++i) {
            r1 = wrap_distance(x[i] - y[i], tree->raw_boxsize_data[i + tree->m], tree->raw_boxsize_data[i]);
            if (NPY_LIKELY(p==2.)) {
                r += r1 * r1;
            } else if (p==infinity) {
                r = dmax(r,dabs(r1));
            } else if (p==1.) {
                r += dabs(r1);
            } else {
                r += std::pow(dabs(r1),p);
            }
            if (r>upperbound) 
                return r;
        } 
        return r;
    } 
};

/*
 * Rectangle-to-rectangle distance tracker
 * =======================================
 *
 * The logical unit that repeats over and over is to keep track of the
 * maximum and minimum distances between points in two hyperrectangles
 * as these rectangles are successively split.
 *
 * Example
 * -------
 * node1 encloses points in rect1, node2 encloses those in rect2
 *
 * cdef RectRectDistanceTracker dist_tracker
 * dist_tracker = RectRectDistanceTracker(rect1, rect2, p)
 *
 * ...
 *
 * if dist_tracker.min_distance < ...:
 *     ...
 *
 * dist_tracker.push_less_of(1, node1)
 * do_something(node1.less, dist_tracker)
 * dist_tracker.pop()
 *
 * dist_tracker.push_greater_of(1, node1)
 * do_something(node1.greater, dist_tracker)
 * dist_tracker.pop()
 *
 * Notice that Point is just a reduced case of Rectangle where
 * mins == maxes. 
 *
 */
 
struct RR_stack_item {
    npy_intp    which;
    npy_intp    split_dim;
    npy_float64 min_along_dim;
    npy_float64 max_along_dim;
    npy_float64 min_distance;
    npy_float64 max_distance;
};    

const npy_intp LESS = 1;
const npy_intp GREATER = 2;

template<typename MinMaxDist> 
    struct RectRectDistanceTracker {
    
    const ckdtree * tree;
    Rectangle rect1; 
    Rectangle rect2;
    npy_float64 p; 
    npy_float64 epsfac;
    npy_float64 upper_bound;
    npy_float64 min_distance;
    npy_float64 max_distance;
    
    npy_intp stack_size;
    npy_intp stack_max_size;
    std::vector<RR_stack_item> stack_arr;
    RR_stack_item *stack;

    void _resize_stack(const npy_intp new_max_size) {
        stack_arr.resize(new_max_size);
        stack = &stack_arr[0];
        stack_max_size = new_max_size;
    };
    
    RectRectDistanceTracker(const ckdtree *_tree, 
                 const Rectangle& _rect1, const Rectangle& _rect2,
                 const npy_float64 _p, const npy_float64 eps, 
                 const npy_float64 _upper_bound)
        : tree(_tree), rect1(_rect1), rect2(_rect2), stack_arr(8) {
    
        const npy_float64 infinity = ::infinity; 
        //FIXME: Why is this line not in other member functions?

        if (rect1.m != rect2.m) {
            const char *msg = "rect1 and rect2 have different dimensions";
            throw std::invalid_argument(msg); // raises ValueError
        }
        
        p = _p;
        
        /* internally we represent all distances as distance ** p */
        if (NPY_LIKELY(p == 2.0))
            upper_bound = _upper_bound * _upper_bound;
        else if ((p != infinity) && (_upper_bound != infinity))
            upper_bound = std::pow(_upper_bound,p);
        else
            upper_bound = _upper_bound;
        
        /* fiddle approximation factor */
        if (NPY_LIKELY(p == 2.0)) {
            npy_float64 tmp = 1. + eps;
            epsfac = 1. / (tmp*tmp);
        }
        else if (eps == 0.)
            epsfac = 1.;
        else if (p == infinity) 
            epsfac = 1. / (1. + eps);
        else
            epsfac = 1. / std::pow((1. + eps), p);

        stack = &stack_arr[0];
        stack_max_size = 8;
        stack_size = 0;

        /* Compute initial min and max distances */
        if (NPY_LIKELY(p == 2.0)) {
            min_distance = 0.;
            max_distance = 0.;
            for(npy_intp i=0; i<rect1.m; ++i) {
                npy_float64 min, max;

                MinMaxDist::interval_interval_2(tree, rect1, rect2, i, &min, &max);
                min_distance += min;
                max_distance += max;
            }
        }
        else if (p == infinity) {
            npy_float64 min, max;

            MinMaxDist::rect_rect_p_inf(tree, rect1, rect2, &min, &max);
            min_distance = min;
            max_distance = max;
        }
        else {
            min_distance = 0.;
            max_distance = 0.;
            for(npy_intp i=0; i<rect1.m; ++i) {
                npy_float64 min, max;

                MinMaxDist::interval_interval_p(tree, rect1, rect2, i, p, &min, &max);
                min_distance += min;
                max_distance += max;
            }
        }
    };
    

    void push(const npy_intp which, const npy_intp direction,
              const npy_intp split_dim, const npy_float64 split_val) {
        
        const npy_float64 p = this->p;
        
        Rectangle *rect;
        if (which == 1)
            rect = &rect1;
        else
            rect = &rect2;

        /* push onto stack */
        if (stack_size == stack_max_size)
            _resize_stack(stack_max_size * 2);
            
        RR_stack_item *item = &stack[stack_size];
        ++stack_size;
        item->which = which;
        item->split_dim = split_dim;
        item->min_distance = min_distance;
        item->max_distance = max_distance;
        item->min_along_dim = rect->mins[split_dim];
        item->max_along_dim = rect->maxes[split_dim];

        /* update min/max distances */
        if (NPY_LIKELY(p == 2.0)) {
            npy_float64 min, max;

            MinMaxDist::interval_interval_2(tree, rect1, rect2, split_dim, &min, &max);
            min_distance -= min;
            max_distance -= max;
        }
        else if (p != infinity) {
            npy_float64 min, max;

            MinMaxDist::interval_interval_p(tree, rect1, rect2, split_dim, p, &min, &max);
            min_distance -= min;
            max_distance -= max;
        }
        
        if (direction == LESS)
            rect->maxes[split_dim] = split_val;
        else
            rect->mins[split_dim] = split_val;

        if (NPY_LIKELY(p == 2.0)) {
            npy_float64 min, max;

            MinMaxDist::interval_interval_2(tree, rect1, rect2, split_dim, &min, &max);
            min_distance += min;
            max_distance += max;
        }
        else if (p != infinity) {
            npy_float64 min, max;

            MinMaxDist::interval_interval_p(tree, rect1, rect2, split_dim, p, &min, &max);
            min_distance += min;
            max_distance += max;
        }
        else {
            npy_float64 min, max;

            MinMaxDist::rect_rect_p_inf(tree, rect1, rect2, &min, &max);
            min_distance = min;
            max_distance = max;
        }     
    };

    inline void push_less_of(const npy_intp which,
                                 const ckdtreenode *node) {
        push(which, LESS, node->split_dim, node->split);
    };
            
    inline void push_greater_of(const npy_intp which,
                                    const ckdtreenode *node) {
        push(which, GREATER, node->split_dim, node->split);
    };
    
    inline void pop() {
        /* pop from stack */
        --stack_size;
        
        /* assert stack_size >= 0 */
        if (NPY_UNLIKELY(stack_size < 0)) {
            const char *msg = "Bad stack size. This error should never occur.";
            throw std::logic_error(msg);
        }
        
        RR_stack_item* item = &stack[stack_size];
        min_distance = item->min_distance;
        max_distance = item->max_distance;

        if (item->which == 1) {
            rect1.mins[item->split_dim] = item->min_along_dim;
            rect1.maxes[item->split_dim] = item->max_along_dim;
        }
        else {
            rect2.mins[item->split_dim] = item->min_along_dim;
            rect2.maxes[item->split_dim] = item->max_along_dim;
        }
    };

};


#endif
