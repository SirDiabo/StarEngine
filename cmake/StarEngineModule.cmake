function(starengine_register_module TARGET_NAME)
    set_property(GLOBAL APPEND PROPERTY STARENGINE_MODULE_LIBRARIES ${TARGET_NAME})
endfunction()
