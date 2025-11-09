#!/bin/sh
SCRIPT_DIR="$(dirname "$0")"
REQUEST_URI="${REQUEST_URI:-}"
PATH_INFO="${PATH_INFO:-}"
QUERY_STRING="${QUERY_STRING:-}"

prepend_query() {
	if [ -n "$QUERY_STRING" ]; then
		QUERY_STRING="$1&$QUERY_STRING"
	else
		QUERY_STRING="$1"
	fi
}

normalize_path() {
	case "$1" in
		*/api.html) echo "/api.html" ;;
		*/api/listChannels) echo "/listChannels" ;;
		*/api/listLivestream*) echo "/listLivestream" ;;
		*/api/info) echo "/info" ;;
		*/api) echo "/" ;;
		*) echo "" ;;
	esac
}

for candidate in "$PATH_INFO" "$(normalize_path "$REQUEST_URI")"; do
	case "$candidate" in
		"/api.html")
			if [ -z "$QUERY_STRING" ]; then
				QUERY_STRING="mode=api"
			fi
			;;
		"/listChannels")
			prepend_query "mode=api&sub=listChannels"
			;;
		"/listLivestream")
			prepend_query "mode=api&sub=listLivestream"
			;;
		"/info")
			prepend_query "mode=api&sub=info"
			;;
		"/")
			prepend_query "mode=api"
			;;
	esac
done

export QUERY_STRING
export DOCUMENT_ROOT="${SCRIPT_DIR}"
export HOME="/opt/api"
exec /opt/api/bin/mt-api "$@"
