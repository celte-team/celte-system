class RetryableException : Exception
{
    public RetryableException(string message) : base(message) { }
}