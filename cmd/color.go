package cmd

func cRed() string {
	if noColor {
		return ""
	}
	return "\033[31m"
}

func cGreen() string {
	if noColor {
		return ""
	}
	return "\033[32m"
}

func cYellow() string {
	if noColor {
		return ""
	}
	return "\033[33m"
}

func cReset() string {
	if noColor {
		return ""
	}
	return "\033[0m"
}
