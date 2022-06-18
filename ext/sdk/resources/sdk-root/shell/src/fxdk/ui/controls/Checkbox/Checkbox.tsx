import React from 'react';
import classnames from 'classnames';
import s from './Checkbox.module.scss';


export interface CheckboxProps {
  value: boolean,
  onChange: (value: boolean) => void,
  label?: React.ReactNode,
  disabled?: boolean,
  className?: string,
}

export const Checkbox = React.memo(function Checkbox(props: CheckboxProps) {
  const {
    value,
    onChange,
    label,
    disabled = false,
    className,
  } = props;

  const handleChange = React.useCallback(() => {
    onChange(!value);
  }, [value, onChange]);

  const rootClassName = classnames(s.root, className, {
    [s.disabled]: disabled,
    [s['with-label']]: !!label,
  });

  return (
    <label className={rootClassName}>
      <input
        type="checkbox"
        checked={value}
        disabled={disabled}
        onChange={handleChange}
      />
      <div className={s.indicator}/>
      {label}
    </label>
  );
});
