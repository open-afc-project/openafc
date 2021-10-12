
#include <algorithm>
#include "MathHelpers.h"

double MathHelpers::tile(double size, double value){
    // This is the number of (positive or negative) wraps occurring
    const int over = std::floor(value / size);
    // Remove the number of wraps
    return value - size * over;
}

double MathHelpers::clamp(double size, double value){
    return std::max(0.0, std::min(size, value));
}

double MathHelpers::mirror(double size, double value){
    // This is the number of (positive or negative) wraps occurring
    const int over = std::floor(value / size);
    // Even wraps simply tile
    if(over % 2 == 0){
        // Remove the number of wraps
        return value - size * over;
    }
    // Odd wraps tile with inversion
    else{
        return size * over - value;
    }
}
