import Foundation

public enum Result {
    case success
    case failure(Error)
    public var isSuccess: Bool {
        switch self {
            case .success:
                return true
            case .failure:
                return false
        }
    }
    public var isFailure: Bool {
        return !isSuccess
    }
    public var error: Error? {
        switch self {
            case .success:
                return nil
            case .failure(let error):
                return error
        }
    }
}

public enum ListenResult {
    case success
    case failure(Error)
    case idle
    public var isSuccess: Bool {
        switch self {
            case .success:
                return true
            default:
                return false
        }
    }
    public var isFailure: Bool {
        switch self {
            case .failure:
                return true
            default:
                return false
        }
    }
    public var isIdle: Bool {
        switch self {
            case .idle:
                return true
            default:
                return false
        }
    }
    public var error: Error? {
        switch self {
            case .failure(let error):
                return error
            default:
                return nil
        }
    }
}