export module contracts;

import std;

export namespace arcade::ui {
    template <class T>
    concept RenderDriver = requires(
        T rd, const T& crd,                      /* concept                      */
        float w, float h,                        /* size                         */
        std::string_view text,                   /* text                         */
        std::vector<
            std::byte, 
            std::pmr::monotonic_buffer_resource
        > res                                     /* vector unpacked resource     */
    ) {
        std::constructible_from<T>;

        { rd.panel(w, h) } -> std::same_as<T&>;
        { rd.text(text)  } -> std::same_as<T&>;
        { rd.image(res)  } -> std::same_as<T&>;
    };

    template <class T, class D>
    concept App = requires(
        T a, const T& ca,       /* concept                  */
        int argc, char** argv,  /* app.run()                */
        D&& driver              /* app.setRenderDriver()    */
    ) {
        std::constructible_from<T>;

        { a.run(argc, argv)         }           -> std::same_as<bool>;
        { a.setRenderDriver(driver) }           -> std::same_as<T&>;
        { ca.version()              } noexcept  -> std::same_as<const char*>;
    } && RenderDriver<D>;
} // namespace arcade::ui