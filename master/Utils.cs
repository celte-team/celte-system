
class Utils
{
    /// <summary>
    /// Retries an action a specified number of times with a delay between attempts.
    /// Attempts are considered failed if an exception is thrown.
    /// </summary>
    /// <param name="action"></param>
    /// <param name="maxRetries"></param>
    /// <param name="delay"></param>
    public static T Retry<T>(Func<T> action, int maxRetries = 3, int delay = 100)
    {
        int attempts = 0;
        while (attempts < maxRetries)
        {
            try
            {
                T result = action();
                return result;
            }
            catch (RetryableException)
            {
                attempts++;
                if (attempts >= maxRetries)
                {
                    throw;
                }
                System.Threading.Thread.Sleep(delay);
            }
            catch (Exception ex)
            {
                throw new Exception($"An error occurred: {ex.Message}", ex);
            }
        }
        throw new InvalidOperationException("Retry failed to return a result.");
    }

    /// <summary>
    /// Logs a message to Redis under the 'master_logs' key.
    /// </summary>
    /// <param name="message"></param>
    public static void LogToRedis(string message)
    {
        RedisDb.SetString("master_logs", $"{DateTime.UtcNow}: {message}");
    }

    public static string GetConfigOption(string key, string defaultValue)
    {
        return Environment.GetEnvironmentVariable(key) ?? defaultValue;
    }
}