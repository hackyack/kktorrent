set(ktupnptest_SRC main.cpp upnptestwidget.cpp)
	

kde4_add_ui_files(ktupnptest_SRC upnptestwidget.ui)
kde4_add_executable(ktupnptest ${ktupnptest_SRC})

target_link_libraries(ktupnptest ${LIBKTORRENT_LIBRARIES} ${QT_QTNETWORK_LIBRARY} ${KDE4_KDEUI_LIBS})
install(TARGETS ktupnptest ${INSTALL_TARGETS_DEFAULT_ARGS})

