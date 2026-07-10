# Copyright (c) 2021 Fritzing GmbH

message("Using fritzing svgpp detect script.")

unix:!macx {
	# On linux, libsvgpp-dev installs to /usr/include/svgpp/
	# which is in standard paths, so we do not need to add anything.
	message("Using system svgpp headers")
} else {
	exists($$absolute_path($$PWD/../../svgpp-1.3.1)) {
		SVGPPPATH = $$absolute_path($$PWD/../../svgpp-1.3.1)
		message("found svgpp in $${SVGPPPATH}")
	}

	message("including $$absolute_path($${SVGPPPATH}/include)")
	INCLUDEPATH += $$absolute_path($${SVGPPPATH}/include)
}
