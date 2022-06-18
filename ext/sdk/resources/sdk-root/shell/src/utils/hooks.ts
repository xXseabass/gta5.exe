import { Api, ApiMessageCallback, ApiMessageListener } from 'fxdk/browser/Api';
import React from 'react';
import { ShellState } from 'store/ShellState';
import { fastRandomId } from './random';

export const useSid = (watchers: React.DependencyList = []) => {
  const initialSid = React.useMemo(fastRandomId, []);
  const sidRef = React.useRef(initialSid);

  React.useEffect(() => {
    sidRef.current = fastRandomId();
  }, watchers); // eslint-disable-line react-hooks/exhaustive-deps

  return sidRef.current;
};

export const useApiMessage = (type: string, cb: ApiMessageListener, deps: React.DependencyList = []) => {
  React.useEffect(() => Api.on(type, cb), deps); // eslint-disable-line react-hooks/exhaustive-deps
};

export const useApiMessageScoped = (type: string, scope: string, cb: ApiMessageListener, deps: React.DependencyList = []) => {
  React.useEffect(() => {
    return Api.onScoped(type, scope, cb);
  }, [...deps, scope]);
};

export const useCounter = (initial: number = 0) => {
  const [counter, setCounter] = React.useState<number>(initial);
  const counterRef = React.useRef(counter);
  counterRef.current = counter;

  const update = React.useCallback((diff: number) => {
    setCounter(counterRef.current + diff);
  }, []);

  const increment = React.useCallback(() => {
    setCounter(counterRef.current + 1);
  }, []);

  const decrement = React.useCallback(() => {
    setCounter(counterRef.current - 1);
  }, []);

  return [counter, increment, decrement, update];
};


export type UseOpenFlagHook = [boolean, () => void, () => void, () => void];

export const useOpenFlag = (defaultValue: boolean = false): UseOpenFlagHook => {
  const [isOpen, setIsOpen] = React.useState(defaultValue);

  const open = React.useCallback(() => {
    setIsOpen(true);
  }, []);
  const close = React.useCallback(() => {
    setIsOpen(false);
  }, []);

  const toggle = React.useCallback(() => {
    setIsOpen(!isOpen);
  }, [isOpen]);

  return [isOpen, open, close, toggle];
};


export const useStore = <T>(defaultValue: Record<string, T>) => {
  const [sentinel, setSentinel] = React.useState({});

  const storeRef = React.useRef(defaultValue);

  const set = React.useCallback((id: string, item: T) => {
    storeRef.current[id] = item;

    setSentinel({});
  }, [setSentinel]);

  const get = React.useCallback((id: string) => {
    return storeRef.current[id];
  }, []);

  const remove = React.useCallback((id: string) => {
    delete storeRef.current[id];

    setSentinel({});
  }, [setSentinel]);

  return {
    store: storeRef.current,
    set,
    get,
    remove,
    ___sentinel: sentinel,
  };
};

export const useDebouncedCallback = <T extends any[], U extends any, R = (...args: T) => any>(
  cb: (...args: T) => U,
  timeout: number,
  watchers: React.DependencyList = [],
): R => {
  const cbRef = React.useRef(cb);
  const timerRef = React.useRef<any>();

  cbRef.current = cb;

  const realCb = (...args: T): any => {
    if (timerRef.current) {
      clearTimeout(timerRef.current);
    }

    timerRef.current = setTimeout(() => {
      timerRef.current = undefined;

      cbRef.current(...args);
    }, timeout);
  };

  React.useEffect(() => () => {
    if (timerRef.current) {
      clearTimeout(timerRef.current);
    }
  }, []);

  return React.useCallback<any>(realCb, []);
};

export interface UseOpenFolderSelectDialogOptions {
  startPath: string,
  dialogTitle: string,
  notOnlyFolders?: boolean,
}

export const useOpenFolderSelectDialog = (options: UseOpenFolderSelectDialogOptions, onSelected: (folderPath: string | null) => void) => {
  const { startPath, dialogTitle, notOnlyFolders = false } = options;

  const callbackRef = React.useRef<((folderPath: string | null) => void) | null>(onSelected);
  callbackRef.current = onSelected;

  React.useEffect(() => () => {
    callbackRef.current = null;
  }, []);

  return React.useCallback(() => {
    if (notOnlyFolders) {
      fxdkOpenSelectFileDialog(startPath, dialogTitle, (folderPath) => callbackRef.current?.(folderPath));
    } else {
      fxdkOpenSelectFolderDialog(startPath, dialogTitle, (folderPath) => callbackRef.current?.(folderPath));
    }
  }, [startPath, dialogTitle, notOnlyFolders]);
};

export const useSendApiMessageCallback = <Data, ResponseData>(type: string, callback: ApiMessageCallback<ResponseData>) => {
  const disposerRef = React.useRef<Function | null>(null);
  const callbackRef = React.useRef<ApiMessageCallback<ResponseData> | null>(callback);
  callbackRef.current = callback;

  React.useEffect(() => () => {
    disposerRef.current?.();

    callbackRef.current = null;
  }, []);

  return React.useCallback(async (data: Data) => {
    if (disposerRef.current) {
      return;
    }

    disposerRef.current = Api.sendCallback(type, data, (error: string | null, response: ResponseData | void) => {
      disposerRef.current = null;

      const cb = callbackRef.current;
      if (cb) {
        if (error) {
          cb(error);
        } else {
          cb(null, response as any);
        }
      }
    });
  }, [type]);
};

export const useOutsideClick = (ref, callback) => {
  const callbackRef = React.useRef(callback);
  callbackRef.current = callback;

  const handleClick = (e) => {
    if (ref.current && !ref.current.contains(e.target)) {
      callbackRef.current();
    }
  };

  React.useEffect(() => {
    document.addEventListener("click", handleClick);

    return () => document.removeEventListener("click", handleClick);
  });
};

export const useIframeCover = () => {
  React.useEffect(() => {
    ShellState.enableIframeCover();

    return () => ShellState.disableIframeCover();
  }, []);
};
