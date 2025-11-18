
JAVAC_VERSION=$(javac --version)
JAVAC_MAIN=${JAVAC_VERSION:6:2}

if [ "$JAVAC_MAIN" -gt 19 ]; then

echo "Java = $JAVAC_VERSION"
# sudo apt-get install openjdk-11-jre
# sudo apt-get install openjdk-11-jdk

# sudo apt-get install openjdk-17-jre
sudo apt-get install openjdk-17-jdk


sudo update-alternatives --config javac

javac --version

else
  echo "JavaC seems to be correct: $JAVAC_VERSION} - Main = $JAVAC_MAIN}"
fi