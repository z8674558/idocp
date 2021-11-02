#ifndef ROBOTOC_LINE_SEARCH_SETTINGS_HPP_
#define ROBOTOC_LINE_SEARCH_SETTINGS_HPP_

#include <string>


namespace robotoc {

///
/// @class LineSearchSettings
/// @brief Settings for the line search. 
///
class LineSearchSettings {
public:
  ///
  /// @brief Construct an object storing line search settings.
  /// @param[in] line_search_method If set to "filter", filter method is used
  /// as a search scheme. If set to "merit-backtracking",
  /// backtracking line search is used.
  /// @param[in] step_size_reduction_rate Reduction rate of the step size. 
  /// @param[in] min_step_size Minimum step size.
  /// @param[in] armijo_control_rate Control rate in Armijo condition. This value 
  /// sets the slope of the linear approximation in the Armijo condition.
  /// @param[in] margin_rate Margin rate to determine penalty parameter in
  /// the merit function.
  ///
  LineSearchSettings(const std::string& line_search_method,
                     const double step_size_reduction_rate, 
                     const double min_step_size, 
                     const double armijo_control_rate,
                     const double margin_rate);

  ///
  /// @brief Default constructor. 
  ///
  LineSearchSettings();

  ///
  /// @brief Destructor. 
  ///
  ~LineSearchSettings();

  ///
  /// @brief Default copy constructor. 
  ///
  LineSearchSettings(const LineSearchSettings&) = default;

  ///
  /// @brief Default copy assign operator. 
  ///
  LineSearchSettings& operator=(const LineSearchSettings&) = default;

  ///
  /// @brief Default move constructor. 
  ///
  LineSearchSettings(LineSearchSettings&&) noexcept = default;

  ///
  /// @brief Default move assign operator. 
  ///
  LineSearchSettings& operator=(LineSearchSettings&&) noexcept = default;

  ///
  /// @brief If set to "filter", filter method is used
  /// as a search scheme. If set to "merit-backtracking",
  /// backtracking line search is used.
  ///
  std::string line_search_method;

  ///
  /// @brief Reduction rate of the step size.
  ///
  double step_size_reduction_rate;

  ///
  /// @brief Minimum step size.
  ///
  double min_step_size;

  ///
  /// @brief Control rate in Armijo condition. This value 
  /// sets the slope of the linear approximation in the Armijo condition.
  ///
  double armijo_control_rate;

  ///
  /// @brief Margin rate to determine penalty parameter in
  /// the merit function.
  double margin_rate;

  ///
  /// @brief Returns settings with default parameters.
  ///
  static LineSearchSettings defaultSettings();
};

} // namespace robotoc


#endif // ROBOTOC_LINE_SEARCH_SETTINGS_HPP_