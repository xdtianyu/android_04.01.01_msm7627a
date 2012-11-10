package junit.runner;

/**
 * An interface to define how a test suite should be loaded.
 *
 * // android-changed
 * @deprecated unused and not present in current upstream JUnit
 */
public interface TestSuiteLoader {
    abstract public Class load(String suiteClassName) throws ClassNotFoundException;
    abstract public Class reload(Class aClass) throws ClassNotFoundException;
}
