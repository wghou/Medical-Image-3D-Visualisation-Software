#ifndef MACROS_HPP
#define MACROS_HPP

/**
This file exists to define some macros used through the program.
**/
#include <glm/glm.hpp>
#include <type_traits>

/// @brief Allows to restrict a template to use only floating point values for the type T.
template <typename T>
using restrict_floating_point_check = typename std::enable_if<std::is_floating_point<T>::value>::type*;

namespace glm {

	template <typename out, typename in, qualifier prec>
	GLM_FUNC_DECL GLM_CONSTEXPR glm::vec<3, out, prec> convert_to(glm::vec<3, in, prec> origin) {
		return glm::vec<3, out, prec>(
			static_cast<out>(origin.x),
			static_cast<out>(origin.y),
			static_cast<out>(origin.z)
		);
	}

	template <typename out, typename in, qualifier prec>
	GLM_FUNC_DECL GLM_CONSTEXPR glm::vec<4, out, prec> convert_to(glm::vec<4, in, prec> origin) {
		return glm::vec<4, out, prec>(
			static_cast<out>(origin.x),
			static_cast<out>(origin.y),
			static_cast<out>(origin.z),
			static_cast<out>(origin.a)
		);
	}

} // glm namespace

#define FUNC_ERR(s) __attribute__((error(s)))
#define FUNC_WARN(s) __attribute__((warning(s)))

#define FUNC_FLATTEN __attribute__((flatten))
#define FUNC_ALWAYS_INLINE __attribute__((always_inline))

/* Copy paste this to have trace output at debug time
#ifndef NDEBUG
	std::cerr << "[TRACE][" << __PRETTY_FUNCTION__ << "] : \n";
#endif
*/

#endif // MACROS_HPP
