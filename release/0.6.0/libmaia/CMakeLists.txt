SET( MAIA_SRCS
	maiaObject.cpp
	maiaFault.cpp
	maiaXmlRpcClient.cpp
)

SET( MAIA_MOC_HDRS
	maiaObject.h
	maiaFault.h
	maiaXmlRpcClient.h
)

QT4_WRAP_CPP( MAIA_MOC_SRCS ${MAIA_MOC_HDRS} )

ADD_LIBRARY( Maia ${MAIA_MOC_SRCS} ${MAIA_SRCS} )

