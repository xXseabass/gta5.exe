import classnames from 'classnames';
import { getPrintableKeystroke, Keystroke as TKeystroke } from 'utils/HotkeyController';
import s from './Keystroke.module.scss';

export interface KeystrokeProps {
  combination: string | TKeystroke,

  className?: string,
}

export function Keystroke(props: KeystrokeProps) {
  const combination = typeof props.combination === 'string'
    ? props.combination
    : getPrintableKeystroke(props.combination);

  const rootClassName = classnames(s.root, props.className);

  return (
    <div className={rootClassName}>
      {combination}
    </div>
  );
}
