# Griddick TNC — /opt/griddick/bin
if [ -d /opt/griddick/bin ]; then
    case ":${PATH}:" in
        *:/opt/griddick/bin:*) ;;
        *) PATH="/opt/griddick/bin:${PATH}" ;;
    esac
    export PATH
fi
