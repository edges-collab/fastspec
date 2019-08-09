a0 = 0.35875;
a1 = 0.48829;
a2 = 0.14128;
a3 = 0.01168;

nsamples = 1024*1024;
halfsamples = nsamples/2;

n = 0:(nsamples-1);
w = a0 ...
    - a1.*cos(2.*pi.*n./(nsamples-1)) ...
    + a2.*cos(4.*pi.*n./(nsamples-1)) ...
    - a3.*cos(6.*pi.*n./(nsamples-1));
    
w_sum = sum(w);
w_long = [w, w];

w2 = w.^2;
w2_sum = sum(w2);
w2_long = [w2, w2];

eff_volt = sum(w)./nsamples;
eff_power = w2_sum./nsamples;
disp(sprintf('Efficiency (voltage): %6.4f', eff_volt));
disp(sprintf('Efficiency (power): %6.4f', eff_power));

% Calculate covariance and efficiency for different numbers
% of window function offsets
for (i=2:16)
    
    offset = floor(nsamples/i);
    wins = zeros(i, nsamples);
    wins2 = zeros(i, nsamples);
    data = repmat(randn(1, nsamples), i, 1);
    
    for (j=1:i)
        index = 1 + (j-1)*offset;
        wins(j,:) = w_long(index:(index+nsamples-1));        
        wins2(j,:) = w2_long(index:(index+nsamples-1));
    end
     
    obs = abs(fft(wins.*data, [], 2)).^2;
    obs = obs(:,1:halfsamples);
    
    all_obs = mean(obs,1);
    
    one_obs = obs(1,:);
    
    ref_obs = abs(fft(data(1,:))).^2;
    ref_obs = ref_obs(:,1:halfsamples);
        
    ref_std = std(ref_obs)./ mean(ref_obs);
    one_std = std(one_obs) ./ mean(one_obs) ./ sqrt(eff_volt);
    this_std = std(all_obs) ./ mean(all_obs) ./ sqrt(eff_volt);
    
    first_obs = obs(1,:);
    other_obs = sum(obs(2:end, :),1);
    
    disp(sprintf('Standard deviation (reference) with %d: %6.4f (%6.4f, %6.4f)', i, this_std, ref_std, one_std));
    
end
    
    
return;
    
    
    
    wprime = w_long(index:(index+nsamples-1));
    thiscorr = (sum(w .* wprime) ./ w2_sum).^2;
    thiseff = eff_power .* i .* (1-thiscorr); 
    disp(sprintf('Correlation at nsamples/%d (power): %6.4f', i, thiscorr));
    disp(sprintf('Total efficiency with %d: %6.4f', i, thiseff));

    
% disp(sprintf('Correlation at nsamples/2: %6.4f', corr_w2));
% disp(sprintf('Correlation at nsamples/2 (power): %6.4f', corr_w2.^2));
% disp(sprintf('Correlation at nsamples/3: %6.4f', corr_w3));
% disp(sprintf('Correlation at nsamples/3 (power): %6.4f', corr_w3.^2));
% disp(sprintf('Correlation at nsamples/4: %6.4f', corr_w4));
% disp(sprintf('Correlation at nsamples/4 (power): %6.4f', corr_w4.^2));

    
% figure(2);clf;
% plot(1:nsamples, w, 1:nsamples, w2, 1:nsamples, w4);
% xlim([1, nsamples]);
% 
% figure(3);clf;
% plot(1:nsamples, w.^2, 1:nsamples, w2.^2, 1:nsamples, w4.^2);
% xlim([1, nsamples]);

% data=randn(1,nsamples);
% out = abs(fft(data.*w)).^2;
% out2 = abs(fft(data.*w2)).^2;
% out4 = abs(fft(data.*w4)).^2;

out_volt = fft(data.*w);
out_half = out_volt(1:(nsamples/2));
out_long = [out_half, out_half];

channel_corr = zeros(1,20);
for (i=1:20)
    channel_corr(i) = abs(sum( out_half .* conj(out_long(i:(i+halfsamples-1))) + conj(out_half) .* out_long(i:(i+halfsamples-1)) )./ 2) ./ sum(out_half.*conj(out_half));
end

figure(4);clf;
plot(1:20, channel_corr);

% cv12 = cov(out, out2)
% cv12(1,2)/cv12(1,1)
% 
% cv24 = cov(out2, out4)
% cv24(1,2)/cv24(1,1)
% 
% cv41 = cov(out4, out)
% cv41(1,2)/cv41(1,1)






